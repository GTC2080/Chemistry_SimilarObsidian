use std::ffi::CString;
use std::os::raw::{c_char, c_int};
use std::path::Path;
use std::sync::Mutex;
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};

use serde::Serialize;
use serde_json::Value;
use tauri::State;

use crate::chem_api::RetroTreeData;
use crate::crystal::{LatticeData, MillerPlaneData};
use crate::kinetics::{KineticsParams, KineticsResult};
use crate::models::{
    EnrichedGraphData, FileTreeNode, GraphData, MolecularPreview, NoteInfo, SpectroscopyData,
    TagInfo, TagTreeNode,
};
use crate::pdf::ink::{RawStroke, SmoothedStroke};
use crate::symmetry::SymmetryData;
use crate::{AppError, AppResult};

mod ffi;
use ffi::*;
mod errors;
use errors::*;
mod defaults;
pub use defaults::*;

#[derive(Default)]
pub struct SealedKernelState {
    session: Mutex<Option<usize>>,
    vault_path: Mutex<Option<String>>,
}

#[derive(Serialize)]
pub struct SealedKernelSessionSummary {
    active: bool,
    vault_path: Option<String>,
    state: Option<SealedKernelStateSnapshot>,
}

#[derive(Serialize)]
pub struct SealedKernelStateSnapshot {
    session_state: i32,
    index_state: i32,
    indexed_note_count: u64,
    pending_recovery_ops: u64,
}

mod types;
use types::*;
pub use types::{AiEmbeddingRefreshJob, SealedKernelStoichiometryInput, SealedKernelTruthAward};

fn snapshot_from_raw(raw: SealedKernelBridgeStateSnapshot) -> SealedKernelStateSnapshot {
    SealedKernelStateSnapshot {
        session_state: raw.session_state,
        index_state: raw.index_state,
        indexed_note_count: raw.indexed_note_count,
        pending_recovery_ops: raw.pending_recovery_ops,
    }
}

fn get_state_for_session(
    session: *mut SealedKernelBridgeSession,
) -> AppResult<SealedKernelStateSnapshot> {
    let mut raw = SealedKernelBridgeStateSnapshot {
        session_state: 0,
        index_state: 0,
        indexed_note_count: 0,
        pending_recovery_ops: 0,
    };
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe { sealed_kernel_bridge_get_state(session, &mut raw, &mut error) };
    if code != 0 {
        return Err(bridge_error("sealed_kernel_get_state", code, error));
    }

    Ok(snapshot_from_raw(raw))
}

fn wait_for_catalog_ready(session: *mut SealedKernelBridgeSession) -> AppResult<()> {
    let deadline = Instant::now() + Duration::from_secs(3);
    loop {
        let snapshot = get_state_for_session(session)?;
        if snapshot.index_state != 1 && snapshot.index_state != 3 {
            return Ok(());
        }
        if Instant::now() >= deadline {
            return Ok(());
        }
        std::thread::sleep(Duration::from_millis(25));
    }
}

fn open_vault_inner(
    vault_path: String,
    state: &SealedKernelState,
) -> AppResult<SealedKernelSessionSummary> {
    if vault_path.trim().is_empty() {
        return Err(AppError::Custom(
            "vault_path must be non-empty.".to_string(),
        ));
    }

    let vault_path_c = CString::new(vault_path.clone())
        .map_err(|_| AppError::Custom("vault_path must not contain NUL bytes.".to_string()))?;
    let mut session: *mut SealedKernelBridgeSession = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_open_vault_utf8(vault_path_c.as_ptr(), &mut session, &mut error)
    };
    if code != 0 {
        return Err(bridge_error("sealed_kernel_open_vault", code, error));
    }

    {
        let mut guard = state.session.lock().map_err(|_| AppError::Lock)?;
        if let Some(existing) = guard.take() {
            unsafe { sealed_kernel_bridge_close(existing as *mut SealedKernelBridgeSession) };
        }
        *guard = Some(session as usize);
    }
    {
        let mut guard = state.vault_path.lock().map_err(|_| AppError::Lock)?;
        *guard = Some(vault_path);
    }

    let state_snapshot = get_state_for_session(session)?;
    Ok(SealedKernelSessionSummary {
        active: true,
        vault_path: state.vault_path.lock().map_err(|_| AppError::Lock)?.clone(),
        state: Some(state_snapshot),
    })
}

pub fn validate_vault_root_path(vault_path: &str) -> AppResult<()> {
    let vault_path_c = CString::new(vault_path)
        .map_err(|_| AppError::Custom("vault_path must not contain NUL bytes.".to_string()))?;
    let mut error: *mut c_char = std::ptr::null_mut();
    let code =
        unsafe { sealed_kernel_bridge_validate_vault_root_utf8(vault_path_c.as_ptr(), &mut error) };
    if code != 0 {
        return Err(bridge_error(
            "sealed_kernel_validate_vault_root",
            code,
            error,
        ));
    }
    Ok(())
}

pub fn ensure_vault_open(vault_path: &str, state: &SealedKernelState) -> AppResult<()> {
    let current = state.vault_path.lock().map_err(|_| AppError::Lock)?.clone();
    let session = {
        let guard = state.session.lock().map_err(|_| AppError::Lock)?;
        *guard
    };
    if session.is_some() && current.as_deref() == Some(vault_path) {
        return Ok(());
    }

    open_vault_inner(vault_path.to_string(), state)?;
    Ok(())
}

pub fn close_vault_state(state: &SealedKernelState) -> AppResult<()> {
    let existing = {
        let mut guard = state.session.lock().map_err(|_| AppError::Lock)?;
        guard.take()
    };
    if let Some(existing) = existing {
        unsafe { sealed_kernel_bridge_close(existing as *mut SealedKernelBridgeSession) };
    }

    {
        let mut guard = state.vault_path.lock().map_err(|_| AppError::Lock)?;
        *guard = None;
    }

    Ok(())
}

fn active_session(state: &SealedKernelState) -> AppResult<*mut SealedKernelBridgeSession> {
    let session = {
        let guard = state.session.lock().map_err(|_| AppError::Lock)?;
        *guard
    };
    session
        .map(|value| value as *mut SealedKernelBridgeSession)
        .ok_or_else(|| AppError::Custom("sealed kernel session is not open.".to_string()))
}

pub fn active_session_token(state: &SealedKernelState) -> AppResult<usize> {
    Ok(active_session(state)? as usize)
}

fn session_from_token(session: usize) -> AppResult<*mut SealedKernelBridgeSession> {
    if session == 0 {
        return Err(AppError::Custom(
            "sealed kernel session is not open.".to_string(),
        ));
    }
    Ok(session as *mut SealedKernelBridgeSession)
}

pub fn active_vault_path(state: &SealedKernelState) -> AppResult<String> {
    state
        .vault_path
        .lock()
        .map_err(|_| AppError::Lock)?
        .clone()
        .ok_or_else(|| AppError::Custom("sealed kernel vault is not open.".to_string()))
}

fn query_note_catalog(
    state: &SealedKernelState,
    limit: u64,
    ignored_roots: Option<&str>,
) -> AppResult<SealedKernelNoteCatalog> {
    if limit == 0 {
        return Err(AppError::Custom(
            "limit must be greater than zero.".to_string(),
        ));
    }

    let session = active_session(state)?;
    let ignored_roots_c = ignored_roots
        .map(|value| cstring_arg(value.to_string(), "ignored_roots"))
        .transpose()?;
    let mut raw_json: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = match ignored_roots_c.as_ref() {
        Some(ignored_roots) => unsafe {
            sealed_kernel_bridge_query_notes_filtered_json(
                session,
                limit,
                ignored_roots.as_ptr(),
                &mut raw_json,
                &mut error,
            )
        },
        None => unsafe {
            sealed_kernel_bridge_query_notes_json(session, limit, &mut raw_json, &mut error)
        },
    };
    if code != 0 {
        return Err(bridge_error("sealed_kernel_query_notes", code, error));
    }

    let value = take_bridge_string(raw_json);
    serde_json::from_str(&value).map_err(|err| {
        AppError::Custom(format!("sealed kernel note catalog JSON is invalid: {err}"))
    })
}

fn query_note_records_with_json<F>(
    operation: &str,
    state: &SealedKernelState,
    f: F,
) -> AppResult<SealedKernelNoteCatalog>
where
    F: FnOnce(*mut SealedKernelBridgeSession, *mut *mut c_char, *mut *mut c_char) -> c_int,
{
    let session = active_session(state)?;
    let mut raw_json: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = f(session, &mut raw_json, &mut error);
    if code != 0 {
        return Err(bridge_error(operation, code, error));
    }

    let value = take_bridge_string(raw_json);
    serde_json::from_str(&value)
        .map_err(|err| AppError::Custom(format!("{operation} returned invalid JSON: {err}")))
}

fn note_info_from_record(vault_path: &str, record: SealedKernelNoteRecord) -> NoteInfo {
    let rel_path = record.rel_path.replace('\\', "/");
    let abs_path = Path::new(vault_path).join(&rel_path);
    let extension = derive_file_extension_from_path(&rel_path).unwrap_or_default();
    let name = derive_note_display_name_from_path(&rel_path)
        .ok()
        .filter(|value| !value.is_empty())
        .unwrap_or_else(|| record.title.clone());
    let updated_at = (record.mtime_ns / 1_000_000_000) as i64;

    NoteInfo {
        id: rel_path,
        name,
        path: abs_path.to_string_lossy().into_owned(),
        created_at: updated_at,
        updated_at,
        file_extension: extension,
    }
}

fn note_info_from_tree_note(vault_path: &str, note: SealedKernelFileTreeNote) -> NoteInfo {
    let rel_path = note.rel_path.replace('\\', "/");
    let abs_path = Path::new(vault_path).join(&rel_path);
    let updated_at = (note.mtime_ns / 1_000_000_000) as i64;

    NoteInfo {
        id: rel_path,
        name: note.name,
        path: abs_path.to_string_lossy().into_owned(),
        created_at: updated_at,
        updated_at,
        file_extension: note.extension,
    }
}

fn file_tree_node_from_kernel(vault_path: &str, node: SealedKernelFileTreeNode) -> FileTreeNode {
    FileTreeNode {
        name: node.name,
        full_name: node.full_name,
        relative_path: node.relative_path,
        is_folder: node.is_folder,
        note: node
            .note
            .map(|note| note_info_from_tree_note(vault_path, note)),
        children: node
            .children
            .into_iter()
            .map(|child| file_tree_node_from_kernel(vault_path, child))
            .collect(),
        file_count: node.file_count,
    }
}

fn tag_tree_node_from_kernel(node: SealedKernelTagTreeNode) -> TagTreeNode {
    TagTreeNode {
        name: node.name,
        full_path: node.full_path,
        count: node.count,
        children: node
            .children
            .into_iter()
            .map(tag_tree_node_from_kernel)
            .collect(),
    }
}

pub fn query_note_infos_filtered(
    vault_path: &str,
    state: &SealedKernelState,
    limit: u64,
    ignored_roots: &str,
) -> AppResult<Vec<NoteInfo>> {
    ensure_vault_open(vault_path, state)?;
    wait_for_catalog_ready(active_session(state)?)?;
    let catalog = query_note_catalog(state, limit, Some(ignored_roots))?;
    Ok(catalog
        .notes
        .into_iter()
        .map(|record| note_info_from_record(vault_path, record))
        .collect())
}

pub fn query_changed_note_infos(
    vault_path: &str,
    state: &SealedKernelState,
    paths: &[String],
) -> AppResult<Vec<NoteInfo>> {
    ensure_vault_open(vault_path, state)?;
    wait_for_catalog_ready(active_session(state)?)?;
    let limit = note_catalog_default_limit()?;
    let joined = paths.join("\n");
    let paths_c = cstring_arg(joined, "changed_paths")?;
    let catalog = query_note_records_with_json(
        "sealed_kernel_query_changed_notes",
        state,
        |session, raw_json, error| unsafe {
            sealed_kernel_bridge_query_changed_notes_json(
                session,
                paths_c.as_ptr(),
                limit,
                raw_json,
                error,
            )
        },
    )?;
    Ok(catalog
        .notes
        .into_iter()
        .map(|record| note_info_from_record(vault_path, record))
        .collect())
}

pub fn query_file_tree(
    vault_path: &str,
    state: &SealedKernelState,
    limit: u64,
    ignored_roots: &str,
) -> AppResult<Vec<FileTreeNode>> {
    ensure_vault_open(vault_path, state)?;
    wait_for_catalog_ready(active_session(state)?)?;

    if limit == 0 {
        return Err(AppError::Custom(
            "limit must be greater than zero.".to_string(),
        ));
    }

    let session = active_session(state)?;
    let ignored_roots = CString::new(ignored_roots).map_err(|_| {
        AppError::Custom("ignored folder list contains an invalid NUL byte.".to_string())
    })?;
    let mut raw_json: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_query_file_tree_json(
            session,
            limit,
            ignored_roots.as_ptr(),
            &mut raw_json,
            &mut error,
        )
    };
    if code != 0 {
        return Err(bridge_error("sealed_kernel_query_file_tree", code, error));
    }

    let value = take_bridge_string(raw_json);
    let catalog: SealedKernelFileTreeCatalog = serde_json::from_str(&value).map_err(|err| {
        AppError::Custom(format!("sealed kernel file tree JSON is invalid: {err}"))
    })?;
    Ok(catalog
        .nodes
        .into_iter()
        .map(|node| file_tree_node_from_kernel(vault_path, node))
        .collect())
}

pub fn filter_supported_vault_paths_filtered(
    paths: &[String],
    ignored_roots: &str,
) -> AppResult<Vec<String>> {
    let joined = paths.join("\n");
    let paths_c = cstring_arg(joined, "changed_paths")?;
    let ignored_roots_c = cstring_arg(ignored_roots.to_string(), "ignored_roots")?;
    let mut raw_json: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_filter_supported_vault_paths_filtered_json(
            paths_c.as_ptr(),
            ignored_roots_c.as_ptr(),
            &mut raw_json,
            &mut error,
        )
    };
    if code != 0 {
        return Err(bridge_error(
            "sealed_kernel_filter_supported_vault_paths_filtered",
            code,
            error,
        ));
    }

    let value = take_bridge_string(raw_json);
    let catalog: SealedKernelPathCatalog = serde_json::from_str(&value).map_err(|err| {
        AppError::Custom(format!(
            "sealed kernel filtered supported vault path JSON is invalid: {err}"
        ))
    })?;
    Ok(catalog.paths)
}

pub fn query_search_note_infos(
    state: &SealedKernelState,
    query: &str,
    limit: u64,
) -> AppResult<Vec<NoteInfo>> {
    let trimmed = query.trim();
    if trimmed.is_empty() {
        return Ok(Vec::new());
    }
    let vault_path = active_vault_path(state)?;
    let query_c = cstring_arg(trimmed.to_string(), "query")?;
    let catalog = query_note_records_with_json(
        "sealed_kernel_query_search_notes",
        state,
        |session, raw_json, error| unsafe {
            sealed_kernel_bridge_query_search_notes_json(
                session,
                query_c.as_ptr(),
                limit,
                raw_json,
                error,
            )
        },
    )?;
    Ok(catalog
        .notes
        .into_iter()
        .map(|record| note_info_from_record(&vault_path, record))
        .collect())
}

pub fn backlink_note_infos(
    state: &SealedKernelState,
    rel_path: &str,
    limit: u64,
) -> AppResult<Vec<NoteInfo>> {
    let trimmed = rel_path.trim();
    if trimmed.is_empty() {
        return Ok(Vec::new());
    }
    let vault_path = active_vault_path(state)?;
    let rel_path_c = cstring_arg(trimmed.to_string(), "rel_path")?;
    let catalog = query_note_records_with_json(
        "sealed_kernel_query_backlinks",
        state,
        |session, raw_json, error| unsafe {
            sealed_kernel_bridge_query_backlinks_json(
                session,
                rel_path_c.as_ptr(),
                limit,
                raw_json,
                error,
            )
        },
    )?;
    Ok(catalog
        .notes
        .into_iter()
        .map(|record| note_info_from_record(&vault_path, record))
        .collect())
}

pub fn query_tag_infos(state: &SealedKernelState, limit: u64) -> AppResult<Vec<TagInfo>> {
    if limit == 0 {
        return Err(AppError::Custom(
            "limit must be greater than zero.".to_string(),
        ));
    }

    let session = active_session(state)?;
    let mut raw_json: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code =
        unsafe { sealed_kernel_bridge_query_tags_json(session, limit, &mut raw_json, &mut error) };
    if code != 0 {
        return Err(bridge_error("sealed_kernel_query_tags", code, error));
    }

    let value = take_bridge_string(raw_json);
    let catalog: SealedKernelTagCatalog = serde_json::from_str(&value)
        .map_err(|err| AppError::Custom(format!("sealed kernel tag JSON is invalid: {err}")))?;
    Ok(catalog
        .tags
        .into_iter()
        .map(|record| TagInfo {
            name: record.name,
            count: record.count,
        })
        .collect())
}

pub fn tag_note_infos(
    state: &SealedKernelState,
    tag: &str,
    limit: u64,
) -> AppResult<Vec<NoteInfo>> {
    let trimmed = tag.trim();
    if trimmed.is_empty() {
        return Ok(Vec::new());
    }

    let vault_path = active_vault_path(state)?;
    let tag_c = cstring_arg(trimmed.to_string(), "tag")?;
    let catalog = query_note_records_with_json(
        "sealed_kernel_query_tag_notes",
        state,
        |session, raw_json, error| unsafe {
            sealed_kernel_bridge_query_tag_notes_json(
                session,
                tag_c.as_ptr(),
                limit,
                raw_json,
                error,
            )
        },
    )?;
    Ok(catalog
        .notes
        .into_iter()
        .map(|record| note_info_from_record(&vault_path, record))
        .collect())
}

pub fn query_tag_tree(state: &SealedKernelState, limit: u64) -> AppResult<Vec<TagTreeNode>> {
    if limit == 0 {
        return Err(AppError::Custom(
            "limit must be greater than zero.".to_string(),
        ));
    }

    let session = active_session(state)?;
    let mut raw_json: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_query_tag_tree_json(session, limit, &mut raw_json, &mut error)
    };
    if code != 0 {
        return Err(bridge_error("sealed_kernel_query_tag_tree", code, error));
    }

    let value = take_bridge_string(raw_json);
    let catalog: SealedKernelTagTreeCatalog = serde_json::from_str(&value).map_err(|err| {
        AppError::Custom(format!("sealed kernel tag tree JSON is invalid: {err}"))
    })?;
    Ok(catalog
        .nodes
        .into_iter()
        .map(tag_tree_node_from_kernel)
        .collect())
}

pub fn query_graph_data(state: &SealedKernelState, limit: u64) -> AppResult<GraphData> {
    if limit == 0 {
        return Err(AppError::Custom(
            "limit must be greater than zero.".to_string(),
        ));
    }

    let session = active_session(state)?;
    let mut raw_json: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code =
        unsafe { sealed_kernel_bridge_query_graph_json(session, limit, &mut raw_json, &mut error) };
    if code != 0 {
        return Err(bridge_error("sealed_kernel_query_graph", code, error));
    }

    let value = take_bridge_string(raw_json);
    serde_json::from_str(&value)
        .map_err(|err| AppError::Custom(format!("sealed kernel graph JSON is invalid: {err}")))
}

pub fn query_enriched_graph_data(
    state: &SealedKernelState,
    limit: u64,
) -> AppResult<EnrichedGraphData> {
    if limit == 0 {
        return Err(AppError::Custom(
            "limit must be greater than zero.".to_string(),
        ));
    }

    let session = active_session(state)?;
    let mut raw_json: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_query_enriched_graph_json(session, limit, &mut raw_json, &mut error)
    };
    if code != 0 {
        return Err(bridge_error(
            "sealed_kernel_query_enriched_graph",
            code,
            error,
        ));
    }

    let value = take_bridge_string(raw_json);
    serde_json::from_str(&value).map_err(|err| {
        AppError::Custom(format!(
            "sealed kernel enriched graph JSON is invalid: {err}"
        ))
    })
}

fn query_chem_spectra_value(state: &SealedKernelState, limit: u64) -> AppResult<Value> {
    if limit == 0 {
        return Err(AppError::Custom(
            "limit must be greater than zero.".to_string(),
        ));
    }

    let session = active_session(state)?;
    let mut raw_json: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_query_chem_spectra_json(session, limit, &mut raw_json, &mut error)
    };
    if code != 0 {
        return Err(bridge_error(
            "sealed_kernel_query_chem_spectra",
            code,
            error,
        ));
    }

    let value = take_bridge_string(raw_json);
    serde_json::from_str(&value).map_err(|err| {
        AppError::Custom(format!(
            "sealed kernel chemistry spectra JSON is invalid: {err}"
        ))
    })
}

fn get_chem_spectrum_value(
    state: &SealedKernelState,
    attachment_rel_path: &str,
) -> AppResult<Value> {
    let trimmed = attachment_rel_path.trim();
    if trimmed.is_empty() {
        return Err(AppError::Custom(
            "attachment_rel_path must be non-empty.".to_string(),
        ));
    }

    let session = active_session(state)?;
    let attachment_rel_path_c = cstring_arg(trimmed.to_string(), "attachment_rel_path")?;
    let mut raw_json: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_get_chem_spectrum_json(
            session,
            attachment_rel_path_c.as_ptr(),
            &mut raw_json,
            &mut error,
        )
    };
    if code != 0 {
        return Err(bridge_error("sealed_kernel_get_chem_spectrum", code, error));
    }

    let value = take_bridge_string(raw_json);
    serde_json::from_str(&value).map_err(|err| {
        AppError::Custom(format!(
            "sealed kernel chemistry spectrum JSON is invalid: {err}"
        ))
    })
}

fn query_note_chem_spectrum_refs_value(
    state: &SealedKernelState,
    note_rel_path: &str,
    limit: u64,
) -> AppResult<Value> {
    if limit == 0 {
        return Err(AppError::Custom(
            "limit must be greater than zero.".to_string(),
        ));
    }

    let note_rel_path = validate_rel_path(note_rel_path, "笔记")?;
    let note_rel_path_c = cstring_arg(note_rel_path, "note_rel_path")?;
    let session = active_session(state)?;
    let mut raw_json: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_query_note_chem_spectrum_refs_json(
            session,
            note_rel_path_c.as_ptr(),
            limit,
            &mut raw_json,
            &mut error,
        )
    };
    if code != 0 {
        return Err(bridge_error(
            "sealed_kernel_query_note_chem_spectrum_refs",
            code,
            error,
        ));
    }

    let value = take_bridge_string(raw_json);
    serde_json::from_str(&value).map_err(|err| {
        AppError::Custom(format!(
            "sealed kernel chemistry spectrum refs JSON is invalid: {err}"
        ))
    })
}

fn query_chem_spectrum_referrers_value(
    state: &SealedKernelState,
    attachment_rel_path: &str,
    limit: u64,
) -> AppResult<Value> {
    if limit == 0 {
        return Err(AppError::Custom(
            "limit must be greater than zero.".to_string(),
        ));
    }

    let attachment_rel_path = validate_rel_path(attachment_rel_path, "附件")?;
    let attachment_rel_path_c = cstring_arg(attachment_rel_path, "attachment_rel_path")?;
    let session = active_session(state)?;
    let mut raw_json: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_query_chem_spectrum_referrers_json(
            session,
            attachment_rel_path_c.as_ptr(),
            limit,
            &mut raw_json,
            &mut error,
        )
    };
    if code != 0 {
        return Err(bridge_error(
            "sealed_kernel_query_chem_spectrum_referrers",
            code,
            error,
        ));
    }

    let value = take_bridge_string(raw_json);
    serde_json::from_str(&value).map_err(|err| {
        AppError::Custom(format!(
            "sealed kernel chemistry spectrum referrers JSON is invalid: {err}"
        ))
    })
}

pub fn compute_truth_diff(
    prev_content: &str,
    curr_content: &str,
    file_extension: &str,
) -> AppResult<Vec<SealedKernelTruthAward>> {
    let extension_c = CString::new(file_extension)
        .map_err(|_| AppError::Custom("Truth diff 文件扩展名包含非法字符".to_string()))?;
    let mut raw_json: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_compute_truth_diff_json(
            prev_content.as_ptr() as *const c_char,
            prev_content.len() as u64,
            curr_content.as_ptr() as *const c_char,
            curr_content.len() as u64,
            extension_c.as_ptr(),
            &mut raw_json,
            &mut error,
        )
    };
    if code != 0 {
        return Err(truth_diff_bridge_error(code, error));
    }

    let value = take_bridge_string(raw_json);
    let parsed: SealedKernelTruthDiffResult = serde_json::from_str(&value).map_err(|err| {
        AppError::Custom(format!("sealed kernel truth diff JSON is invalid: {err}"))
    })?;
    Ok(parsed.awards)
}

pub fn build_semantic_context(content: &str) -> AppResult<String> {
    let mut raw_text: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_build_semantic_context_text(
            content.as_ptr() as *const c_char,
            content.len() as u64,
            &mut raw_text,
            &mut error,
        )
    };
    if code != 0 {
        return Err(semantic_context_bridge_error(code, error));
    }

    Ok(take_bridge_string(raw_text))
}

pub fn normalize_ai_embedding_text(text: &str) -> AppResult<String> {
    let mut raw_text: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_normalize_ai_embedding_text(
            text.as_ptr() as *const c_char,
            text.len() as u64,
            &mut raw_text,
            &mut error,
        )
    };
    if code != 0 {
        return Err(embedding_text_bridge_error(code, error));
    }
    Ok(take_bridge_string(raw_text))
}

pub fn derive_file_extension_from_path(path: &str) -> AppResult<String> {
    let mut raw_text: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_derive_file_extension_from_path_text(
            path.as_ptr() as *const c_char,
            path.len() as u64,
            &mut raw_text,
            &mut error,
        )
    };
    if code != 0 {
        return Err(product_text_bridge_error(
            "sealed_kernel_derive_file_extension_from_path",
            code,
            error,
        ));
    }
    Ok(take_bridge_string(raw_text))
}

pub fn normalize_vault_relative_path(rel_path: &str) -> AppResult<String> {
    let mut raw_text: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_normalize_vault_relative_path_text(
            rel_path.as_ptr() as *const c_char,
            rel_path.len() as u64,
            &mut raw_text,
            &mut error,
        )
    };
    if code != 0 {
        return Err(bridge_error(
            "sealed_kernel_normalize_vault_relative_path",
            code,
            error,
        ));
    }
    Ok(take_bridge_string(raw_text))
}

pub fn derive_note_display_name_from_path(path: &str) -> AppResult<String> {
    let mut raw_text: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_derive_note_display_name_from_path_text(
            path.as_ptr() as *const c_char,
            path.len() as u64,
            &mut raw_text,
            &mut error,
        )
    };
    if code != 0 {
        return Err(product_text_bridge_error(
            "sealed_kernel_derive_note_display_name_from_path",
            code,
            error,
        ));
    }
    Ok(take_bridge_string(raw_text))
}

#[cfg(test)]
pub fn normalize_database_column_type(column_type: &str) -> AppResult<String> {
    let mut raw_text: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_normalize_database_column_type_text(
            column_type.as_ptr() as *const c_char,
            column_type.len() as u64,
            &mut raw_text,
            &mut error,
        )
    };
    if code != 0 {
        return Err(product_text_bridge_error(
            "sealed_kernel_normalize_database_column_type",
            code,
            error,
        ));
    }
    Ok(take_bridge_string(raw_text))
}

pub fn normalize_database_json<T>(input: &Value) -> AppResult<T>
where
    T: serde::de::DeserializeOwned,
{
    let raw = serde_json::to_string(input)
        .map_err(|err| AppError::Custom(format!("database payload JSON is invalid: {err}")))?;
    let mut raw_json: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_normalize_database_json(
            raw.as_ptr() as *const c_char,
            raw.len() as u64,
            &mut raw_json,
            &mut error,
        )
    };
    if code != 0 {
        return Err(product_text_bridge_error(
            "sealed_kernel_normalize_database_json",
            code,
            error,
        ));
    }
    let value = take_bridge_string(raw_json);
    serde_json::from_str(&value)
        .map_err(|err| AppError::Custom(format!("sealed kernel database JSON is invalid: {err}")))
}

pub fn build_paper_compile_plan(
    workspace: &str,
    template: &str,
    image_paths: &[String],
    csl_path: Option<&str>,
    bibliography_path: Option<&str>,
    resource_separator: &str,
) -> AppResult<PaperCompilePlan> {
    let image_path_ptrs: Vec<*const c_char> = image_paths
        .iter()
        .map(|path| path.as_ptr() as *const c_char)
        .collect();
    let image_path_sizes: Vec<u64> = image_paths.iter().map(|path| path.len() as u64).collect();
    let csl_path = csl_path.unwrap_or("");
    let bibliography_path = bibliography_path.unwrap_or("");

    let mut raw_json: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_build_paper_compile_plan_json(
            workspace.as_ptr() as *const c_char,
            workspace.len() as u64,
            template.as_ptr() as *const c_char,
            template.len() as u64,
            image_path_ptrs.as_ptr(),
            image_path_sizes.as_ptr(),
            image_paths.len() as u64,
            csl_path.as_ptr() as *const c_char,
            csl_path.len() as u64,
            bibliography_path.as_ptr() as *const c_char,
            bibliography_path.len() as u64,
            resource_separator.as_ptr() as *const c_char,
            resource_separator.len() as u64,
            &mut raw_json,
            &mut error,
        )
    };
    if code != 0 {
        return Err(product_text_bridge_error(
            "sealed_kernel_build_paper_compile_plan_json",
            code,
            error,
        ));
    }

    let value = take_bridge_string(raw_json);
    serde_json::from_str(&value).map_err(|err| {
        AppError::Custom(format!(
            "sealed kernel paper compile plan JSON is invalid: {err}"
        ))
    })
}

pub fn default_paper_template() -> AppResult<String> {
    let mut raw_template: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code =
        unsafe { sealed_kernel_bridge_get_default_paper_template(&mut raw_template, &mut error) };
    if code != 0 {
        return Err(product_text_bridge_error(
            "sealed_kernel_get_default_paper_template",
            code,
            error,
        ));
    }
    Ok(take_bridge_string(raw_template))
}

pub fn summarize_paper_compile_log(
    log: &str,
    log_char_limit: u64,
) -> AppResult<PaperCompileLogSummary> {
    let mut raw_json: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_summarize_paper_compile_log_json(
            log.as_ptr() as *const c_char,
            log.len() as u64,
            log_char_limit,
            &mut raw_json,
            &mut error,
        )
    };
    if code != 0 {
        return Err(product_text_bridge_error(
            "sealed_kernel_summarize_paper_compile_log_json",
            code,
            error,
        ));
    }

    let value = take_bridge_string(raw_json);
    serde_json::from_str(&value).map_err(|err| {
        AppError::Custom(format!(
            "sealed kernel paper compile log summary JSON is invalid: {err}"
        ))
    })
}

pub fn normalize_pubchem_query(query: &str) -> AppResult<String> {
    let mut raw_text: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_normalize_pubchem_query_text(
            query.as_ptr() as *const c_char,
            query.len() as u64,
            &mut raw_text,
            &mut error,
        )
    };
    if code != 0 {
        return Err(product_text_bridge_error(
            "sealed_kernel_normalize_pubchem_query_text",
            code,
            error,
        ));
    }
    Ok(take_bridge_string(raw_text))
}

pub fn build_pubchem_compound_info(
    query: &str,
    formula: &str,
    molecular_weight: f64,
    density: Option<f64>,
    property_count: u64,
) -> AppResult<PubChemCompoundInfo> {
    let (has_density, density_value) = match density {
        Some(value) => (1u8, value),
        None => (0u8, 0.0),
    };
    let mut raw_json: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_build_pubchem_compound_info_json(
            query.as_ptr() as *const c_char,
            query.len() as u64,
            formula.as_ptr() as *const c_char,
            formula.len() as u64,
            molecular_weight,
            has_density,
            density_value,
            property_count,
            &mut raw_json,
            &mut error,
        )
    };
    if code != 0 {
        return Err(product_text_bridge_error(
            "sealed_kernel_build_pubchem_compound_info_json",
            code,
            error,
        ));
    }

    let value = take_bridge_string(raw_json);
    serde_json::from_str(&value).map_err(|err| {
        AppError::Custom(format!(
            "sealed kernel PubChem compound info JSON is invalid: {err}"
        ))
    })
}

#[cfg(test)]
pub fn compute_truth_state_from_activity(
    activities: &[(String, i64)],
) -> AppResult<SealedKernelTruthState> {
    let note_ids: Vec<CString> = activities
        .iter()
        .map(|(note_id, _)| {
            CString::new(note_id.as_str()).map_err(|_| {
                AppError::Custom("Truth state note id contains an invalid NUL byte.".to_string())
            })
        })
        .collect::<AppResult<Vec<_>>>()?;
    let note_ptrs: Vec<*const c_char> = note_ids.iter().map(|value| value.as_ptr()).collect();
    let active_secs: Vec<i64> = activities.iter().map(|(_, secs)| *secs).collect();

    let mut raw_json: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_compute_truth_state_json(
            note_ptrs.as_ptr(),
            active_secs.as_ptr(),
            activities.len() as u64,
            &mut raw_json,
            &mut error,
        )
    };
    if code != 0 {
        return Err(bridge_error(
            "sealed_kernel_compute_truth_state",
            code,
            error,
        ));
    }

    let value = take_bridge_string(raw_json);
    serde_json::from_str(&value).map_err(|err| {
        AppError::Custom(format!("sealed kernel truth state JSON is invalid: {err}"))
    })
}

#[cfg(test)]
pub fn compute_study_streak_days_from_timestamps(
    started_at_epoch_secs: &[i64],
    today_bucket: i64,
) -> AppResult<i64> {
    let mut streak_days = 0;
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_compute_study_streak_days_from_timestamps(
            started_at_epoch_secs.as_ptr(),
            started_at_epoch_secs.len() as u64,
            today_bucket,
            &mut streak_days,
            &mut error,
        )
    };
    if code != 0 {
        return Err(bridge_error(
            "sealed_kernel_compute_study_streak_days_from_timestamps",
            code,
            error,
        ));
    }
    Ok(streak_days)
}

#[cfg(test)]
pub fn compute_study_stats_window(
    now_epoch_secs: i64,
    days_back: i64,
) -> AppResult<SealedKernelStudyStatsWindow> {
    let mut today_start_epoch_secs = 0;
    let mut today_bucket = 0;
    let mut week_start_epoch_secs = 0;
    let mut daily_window_start_epoch_secs = 0;
    let mut heatmap_start_epoch_secs = 0;
    let mut folder_rank_limit = 0;
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_compute_study_stats_window(
            now_epoch_secs,
            days_back,
            &mut today_start_epoch_secs,
            &mut today_bucket,
            &mut week_start_epoch_secs,
            &mut daily_window_start_epoch_secs,
            &mut heatmap_start_epoch_secs,
            &mut folder_rank_limit,
            &mut error,
        )
    };
    if code != 0 {
        return Err(bridge_error(
            "sealed_kernel_compute_study_stats_window",
            code,
            error,
        ));
    }

    Ok(SealedKernelStudyStatsWindow {
        today_start_epoch_secs,
        today_bucket,
        week_start_epoch_secs,
        daily_window_start_epoch_secs,
        heatmap_start_epoch_secs,
        folder_rank_limit,
    })
}

#[cfg(test)]
pub fn build_study_heatmap_grid(
    days: &[(String, i64)],
    now_epoch_secs: i64,
) -> AppResult<SealedKernelHeatmapGrid> {
    let dates: Vec<CString> = days
        .iter()
        .map(|(date, _)| {
            CString::new(date.as_str()).map_err(|_| {
                AppError::Custom("Heatmap date contains an invalid NUL byte.".to_string())
            })
        })
        .collect::<AppResult<Vec<_>>>()?;
    let date_ptrs: Vec<*const c_char> = dates.iter().map(|value| value.as_ptr()).collect();
    let active_secs: Vec<i64> = days.iter().map(|(_, secs)| *secs).collect();

    let mut raw_json: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_build_study_heatmap_grid_json(
            date_ptrs.as_ptr(),
            active_secs.as_ptr(),
            days.len() as u64,
            now_epoch_secs,
            &mut raw_json,
            &mut error,
        )
    };
    if code != 0 {
        return Err(bridge_error(
            "sealed_kernel_build_study_heatmap_grid",
            code,
            error,
        ));
    }

    let value = take_bridge_string(raw_json);
    serde_json::from_str(&value).map_err(|err| {
        AppError::Custom(format!(
            "sealed kernel study heatmap grid JSON is invalid: {err}"
        ))
    })
}

pub fn start_study_session(
    note_id: &str,
    folder: &str,
    state: &SealedKernelState,
) -> AppResult<i64> {
    let note_id_c = cstring_arg(note_id.to_string(), "note_id")?;
    let folder_c = cstring_arg(folder.to_string(), "folder")?;
    let session = active_session(state)?;
    let mut session_id = 0i64;
    call_status_operation("sealed_kernel_start_study_session", |error| unsafe {
        sealed_kernel_bridge_start_study_session(
            session,
            note_id_c.as_ptr(),
            folder_c.as_ptr(),
            &mut session_id,
            error,
        )
    })?;
    Ok(session_id)
}

pub fn tick_study_session(
    session_id: i64,
    active_secs: i64,
    state: &SealedKernelState,
) -> AppResult<()> {
    let session = active_session(state)?;
    call_status_operation("sealed_kernel_tick_study_session", |error| unsafe {
        sealed_kernel_bridge_tick_study_session(session, session_id, active_secs, error)
    })
}

pub fn end_study_session(
    session_id: i64,
    active_secs: i64,
    state: &SealedKernelState,
) -> AppResult<()> {
    let session = active_session(state)?;
    call_status_operation("sealed_kernel_end_study_session", |error| unsafe {
        sealed_kernel_bridge_end_study_session(session, session_id, active_secs, error)
    })
}

pub fn query_study_stats(days_back: i64, state: &SealedKernelState) -> AppResult<Value> {
    let session = active_session(state)?;
    let now_secs = unix_now_secs()?;
    call_json_value_operation(
        "sealed_kernel_query_study_stats",
        |out_json, error| unsafe {
            sealed_kernel_bridge_query_study_stats_json(
                session, now_secs, days_back, out_json, error,
            )
        },
    )
}

pub fn query_study_truth_state(state: &SealedKernelState) -> AppResult<Value> {
    let session = active_session(state)?;
    let now_millis = unix_now_millis()?;
    call_json_value_operation(
        "sealed_kernel_query_study_truth_state",
        |out_json, error| unsafe {
            sealed_kernel_bridge_query_study_truth_state_json(session, now_millis, out_json, error)
        },
    )
}

pub fn query_study_heatmap_grid(state: &SealedKernelState) -> AppResult<Value> {
    let session = active_session(state)?;
    let now_secs = unix_now_secs()?;
    call_json_value_operation(
        "sealed_kernel_query_study_heatmap_grid",
        |out_json, error| unsafe {
            sealed_kernel_bridge_query_study_heatmap_grid_json(session, now_secs, out_json, error)
        },
    )
}

pub fn parse_spectroscopy_from_text(raw: &str, extension: &str) -> AppResult<SpectroscopyData> {
    let extension_c = cstring_arg(extension.to_string(), "extension")?;
    let mut raw_json: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_parse_spectroscopy_text_json(
            raw.as_ptr() as *const c_char,
            raw.len() as u64,
            extension_c.as_ptr(),
            &mut raw_json,
            &mut error,
        )
    };
    if code != 0 {
        return Err(spectroscopy_bridge_error(extension, code, error));
    }

    let value = take_bridge_string(raw_json);
    serde_json::from_str(&value).map_err(|err| {
        AppError::Custom(format!("sealed kernel spectroscopy JSON is invalid: {err}"))
    })
}

pub fn generate_mock_retrosynthesis(target_smiles: &str, depth: u8) -> AppResult<RetroTreeData> {
    let target_smiles_c = CString::new(target_smiles)
        .map_err(|_| AppError::Custom("目标分子 SMILES 包含无效字符".to_string()))?;
    let mut raw_json: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_generate_mock_retrosynthesis_json(
            target_smiles_c.as_ptr(),
            depth,
            &mut raw_json,
            &mut error,
        )
    };
    if code != 0 {
        return Err(retrosynthesis_bridge_error(code, error));
    }

    let value = take_bridge_string(raw_json);
    serde_json::from_str(&value).map_err(|err| {
        AppError::Custom(format!(
            "sealed kernel retrosynthesis JSON is invalid: {err}"
        ))
    })
}

pub fn simulate_polymerization_kinetics(params: KineticsParams) -> AppResult<KineticsResult> {
    let mut raw_json: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_simulate_polymerization_kinetics_json(
            params.m0,
            params.i0,
            params.cta0,
            params.kd,
            params.kp,
            params.kt,
            params.ktr,
            params.time_max,
            params.steps as u64,
            &mut raw_json,
            &mut error,
        )
    };
    if code != 0 {
        return Err(kinetics_bridge_error(code, error));
    }

    let value = take_bridge_string(raw_json);
    serde_json::from_str(&value)
        .map_err(|err| AppError::Custom(format!("sealed kernel kinetics JSON is invalid: {err}")))
}

fn f64_ptr(values: &[f64]) -> *const f64 {
    if values.is_empty() {
        std::ptr::null()
    } else {
        values.as_ptr()
    }
}

fn u8_ptr(values: &[u8]) -> *const u8 {
    if values.is_empty() {
        std::ptr::null()
    } else {
        values.as_ptr()
    }
}

pub fn recalculate_stoichiometry(
    rows: &[SealedKernelStoichiometryInput],
) -> AppResult<Vec<SealedKernelStoichiometryRow>> {
    let mw: Vec<f64> = rows.iter().map(|row| row.mw).collect();
    let eq: Vec<f64> = rows.iter().map(|row| row.eq).collect();
    let moles: Vec<f64> = rows.iter().map(|row| row.moles).collect();
    let mass: Vec<f64> = rows.iter().map(|row| row.mass).collect();
    let volume: Vec<f64> = rows.iter().map(|row| row.volume).collect();
    let density: Vec<f64> = rows.iter().map(|row| row.density).collect();
    let has_density: Vec<u8> = rows.iter().map(|row| u8::from(row.has_density)).collect();
    let is_reference: Vec<u8> = rows.iter().map(|row| u8::from(row.is_reference)).collect();

    let mut raw_json: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_recalculate_stoichiometry_json(
            f64_ptr(&mw),
            f64_ptr(&eq),
            f64_ptr(&moles),
            f64_ptr(&mass),
            f64_ptr(&volume),
            f64_ptr(&density),
            u8_ptr(&has_density),
            u8_ptr(&is_reference),
            rows.len() as u64,
            &mut raw_json,
            &mut error,
        )
    };
    if code != 0 {
        return Err(stoichiometry_bridge_error(code, error));
    }

    let value = take_bridge_string(raw_json);
    let parsed: SealedKernelStoichiometryResult = serde_json::from_str(&value).map_err(|err| {
        AppError::Custom(format!(
            "sealed kernel stoichiometry JSON is invalid: {err}"
        ))
    })?;
    if parsed.rows.len() != rows.len() {
        return Err(AppError::Custom(
            "sealed kernel stoichiometry row count mismatch".to_string(),
        ));
    }
    Ok(parsed.rows)
}

pub fn compute_pdf_file_lightweight_hash_for_session(
    session: usize,
    file_path: &str,
) -> AppResult<String> {
    let session = session_from_token(session)?;
    let mut raw_hash: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_compute_pdf_file_lightweight_hash(
            session,
            file_path.as_ptr() as *const c_char,
            file_path.len() as u64,
            &mut raw_hash,
            &mut error,
        )
    };
    if code != 0 {
        return Err(bridge_error(
            "sealed_kernel_compute_pdf_file_lightweight_hash",
            code,
            error,
        ));
    }
    Ok(take_bridge_string(raw_hash))
}

pub fn read_pdf_annotation_json_for_session(
    session: usize,
    pdf_rel_path: &str,
) -> AppResult<String> {
    let session = session_from_token(session)?;
    let pdf_rel_path_c = cstring_arg(pdf_rel_path.to_string(), "pdf_rel_path")?;
    let mut raw_json: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_read_pdf_annotation_json(
            session,
            pdf_rel_path_c.as_ptr(),
            &mut raw_json,
            &mut error,
        )
    };
    if code != 0 {
        return Err(bridge_error(
            "sealed_kernel_read_pdf_annotation_json",
            code,
            error,
        ));
    }
    Ok(take_bridge_string(raw_json))
}

pub fn write_pdf_annotation_json_for_session(
    session: usize,
    pdf_rel_path: &str,
    json: &str,
) -> AppResult<()> {
    let session = session_from_token(session)?;
    let pdf_rel_path_c = cstring_arg(pdf_rel_path.to_string(), "pdf_rel_path")?;
    let json_ptr = if json.is_empty() {
        std::ptr::null()
    } else {
        json.as_ptr() as *const c_char
    };
    call_status_operation("sealed_kernel_write_pdf_annotation_json", |error| unsafe {
        sealed_kernel_bridge_write_pdf_annotation_json(
            session,
            pdf_rel_path_c.as_ptr(),
            json_ptr,
            json.len() as u64,
            error,
        )
    })
}

pub fn smooth_ink_strokes(
    strokes: Vec<RawStroke>,
    tolerance: f32,
) -> AppResult<Vec<SmoothedStroke>> {
    let point_counts: Vec<u64> = strokes
        .iter()
        .map(|stroke| stroke.points.len() as u64)
        .collect();
    let stroke_widths: Vec<f32> = strokes.iter().map(|stroke| stroke.stroke_width).collect();
    let total_points = strokes
        .iter()
        .map(|stroke| stroke.points.len())
        .sum::<usize>();

    let mut xs = Vec::with_capacity(total_points);
    let mut ys = Vec::with_capacity(total_points);
    let mut pressures = Vec::with_capacity(total_points);
    for stroke in &strokes {
        for point in &stroke.points {
            xs.push(point.x);
            ys.push(point.y);
            pressures.push(point.pressure);
        }
    }

    let points_ptr = |values: &Vec<f32>| {
        if values.is_empty() {
            std::ptr::null()
        } else {
            values.as_ptr()
        }
    };
    let counts_ptr = if point_counts.is_empty() {
        std::ptr::null()
    } else {
        point_counts.as_ptr()
    };
    let widths_ptr = if stroke_widths.is_empty() {
        std::ptr::null()
    } else {
        stroke_widths.as_ptr()
    };

    let mut raw_json: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_smooth_ink_strokes_json(
            points_ptr(&xs),
            points_ptr(&ys),
            points_ptr(&pressures),
            counts_ptr,
            widths_ptr,
            strokes.len() as u64,
            tolerance,
            &mut raw_json,
            &mut error,
        )
    };
    if code != 0 {
        return Err(ink_smoothing_bridge_error(code, error));
    }

    let value = take_bridge_string(raw_json);
    serde_json::from_str(&value).map_err(|err| {
        AppError::Custom(format!(
            "sealed kernel ink smoothing JSON is invalid: {err}"
        ))
    })
}

pub fn build_molecular_preview_from_text(
    raw: &str,
    extension: &str,
    max_atoms: usize,
) -> AppResult<MolecularPreview> {
    let extension_c = cstring_arg(extension.to_string(), "extension")?;
    let mut raw_json: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_build_molecular_preview_json(
            raw.as_ptr() as *const c_char,
            raw.len() as u64,
            extension_c.as_ptr(),
            max_atoms as u64,
            &mut raw_json,
            &mut error,
        )
    };
    if code != 0 {
        return Err(molecular_preview_bridge_error(extension, code, error));
    }

    let value = take_bridge_string(raw_json);
    serde_json::from_str(&value).map_err(|err| {
        AppError::Custom(format!(
            "sealed kernel molecular preview JSON is invalid: {err}"
        ))
    })
}

pub fn calculate_symmetry_from_text(raw_data: &str, format: &str) -> AppResult<SymmetryData> {
    let max_atoms = symmetry_atom_limit()?;
    let format_c = cstring_arg(format.to_string(), "format")?;
    let mut raw_json: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_calculate_symmetry_json(
            raw_data.as_ptr() as *const c_char,
            raw_data.len() as u64,
            format_c.as_ptr(),
            max_atoms as u64,
            &mut raw_json,
            &mut error,
        )
    };
    if code != 0 {
        return Err(symmetry_bridge_error(format, max_atoms, code, error));
    }

    let value = take_bridge_string(raw_json);
    serde_json::from_str(&value)
        .map_err(|err| AppError::Custom(format!("sealed kernel symmetry JSON is invalid: {err}")))
}

pub fn build_lattice_from_cif(cif_text: &str, nx: u32, ny: u32, nz: u32) -> AppResult<LatticeData> {
    let mut raw_json: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_build_lattice_from_cif_json(
            cif_text.as_ptr() as *const c_char,
            cif_text.len() as u64,
            nx,
            ny,
            nz,
            &mut raw_json,
            &mut error,
        )
    };
    if code != 0 {
        return Err(crystal_lattice_bridge_error(
            code,
            error,
            crystal_supercell_atom_limit,
        ));
    }

    let value = take_bridge_string(raw_json);
    serde_json::from_str(&value)
        .map_err(|err| AppError::Custom(format!("sealed kernel lattice JSON is invalid: {err}")))
}

pub fn calculate_miller_plane_from_cif(
    cif_text: &str,
    h: i32,
    k: i32,
    l: i32,
) -> AppResult<MillerPlaneData> {
    let mut raw_json: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_calculate_miller_plane_from_cif_json(
            cif_text.as_ptr() as *const c_char,
            cif_text.len() as u64,
            h,
            k,
            l,
            &mut raw_json,
            &mut error,
        )
    };
    if code != 0 {
        return Err(crystal_miller_bridge_error(code, error));
    }

    let value = take_bridge_string(raw_json);
    serde_json::from_str(&value).map_err(|err| {
        AppError::Custom(format!("sealed kernel Miller plane JSON is invalid: {err}"))
    })
}

fn cstring_arg(value: String, label: &str) -> AppResult<CString> {
    CString::new(value)
        .map_err(|_| AppError::Custom(format!("{label} must not contain NUL bytes.")))
}

fn unix_now_secs() -> AppResult<i64> {
    Ok(SystemTime::now().duration_since(UNIX_EPOCH)?.as_secs() as i64)
}

fn unix_now_millis() -> AppResult<i64> {
    Ok(SystemTime::now().duration_since(UNIX_EPOCH)?.as_millis() as i64)
}

fn call_status_operation<F>(operation: &str, f: F) -> AppResult<()>
where
    F: FnOnce(*mut *mut c_char) -> c_int,
{
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = f(&mut error);
    if code != 0 {
        return Err(bridge_error(operation, code, error));
    }
    Ok(())
}

fn parse_bridge_json_value(operation: &str, raw_json: *mut c_char) -> AppResult<Value> {
    let value = take_bridge_string(raw_json);
    serde_json::from_str(&value)
        .map_err(|err| AppError::Custom(format!("{operation} JSON is invalid: {err}")))
}

fn call_json_value_operation<F>(operation: &str, f: F) -> AppResult<Value>
where
    F: FnOnce(*mut *mut c_char, *mut *mut c_char) -> c_int,
{
    let mut raw_json: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = f(&mut raw_json, &mut error);
    if code != 0 {
        return Err(bridge_error(operation, code, error));
    }
    parse_bridge_json_value(operation, raw_json)
}

mod ai;
pub use ai::*;

mod files;
use files::validate_rel_path;
pub use files::*;
#[cfg(test)]
use files::{rel_path_from_file_path, rel_path_from_optional_folder_path};

mod commands;
pub use commands::*;

#[cfg(test)]
mod tests;
