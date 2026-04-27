use std::collections::HashMap;
use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_int};
use std::path::{Path, PathBuf};
use std::sync::Mutex;
use std::time::{Duration, Instant};

use serde::{Deserialize, Serialize};
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

#[repr(C)]
struct SealedKernelBridgeSession {
    _private: [u8; 0],
}

#[repr(C)]
#[derive(Clone, Copy)]
struct SealedKernelBridgeStateSnapshot {
    session_state: i32,
    index_state: i32,
    indexed_note_count: u64,
    pending_recovery_ops: u64,
}

extern "C" {
    fn sealed_kernel_bridge_info_json() -> *mut c_char;
    fn sealed_kernel_bridge_free_string(value: *mut c_char);
    fn sealed_kernel_bridge_open_vault_utf8(
        vault_path_utf8: *const c_char,
        out_session: *mut *mut SealedKernelBridgeSession,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_close(session: *mut SealedKernelBridgeSession);
    fn sealed_kernel_bridge_get_state(
        session: *mut SealedKernelBridgeSession,
        out_state: *mut SealedKernelBridgeStateSnapshot,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_get_note_catalog_default_limit(
        out_limit: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_get_note_query_default_limit(
        out_limit: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_get_vault_scan_default_limit(
        out_limit: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_query_notes_json(
        session: *mut SealedKernelBridgeSession,
        limit: u64,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_query_notes_filtered_json(
        session: *mut SealedKernelBridgeSession,
        limit: u64,
        ignored_roots_utf8: *const c_char,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_get_file_tree_default_limit(
        out_limit: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_query_file_tree_json(
        session: *mut SealedKernelBridgeSession,
        limit: u64,
        ignored_roots_utf8: *const c_char,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_filter_changed_markdown_paths_json(
        changed_paths_lf_utf8: *const c_char,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_filter_supported_vault_paths_filtered_json(
        changed_paths_lf_utf8: *const c_char,
        ignored_roots_utf8: *const c_char,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_read_note_json(
        session: *mut SealedKernelBridgeSession,
        rel_path_utf8: *const c_char,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_write_note_json(
        session: *mut SealedKernelBridgeSession,
        rel_path_utf8: *const c_char,
        content_utf8: *const c_char,
        content_size: u64,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_query_search_notes_json(
        session: *mut SealedKernelBridgeSession,
        query_utf8: *const c_char,
        limit: u64,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_get_search_note_default_limit(
        out_limit: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_get_backlink_default_limit(
        out_limit: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_get_tag_catalog_default_limit(
        out_limit: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_get_tag_note_default_limit(
        out_limit: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_get_tag_tree_default_limit(
        out_limit: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_get_graph_default_limit(
        out_limit: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_query_tags_json(
        session: *mut SealedKernelBridgeSession,
        limit: u64,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_query_tag_notes_json(
        session: *mut SealedKernelBridgeSession,
        tag_utf8: *const c_char,
        limit: u64,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_query_graph_json(
        session: *mut SealedKernelBridgeSession,
        limit: u64,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_query_backlinks_json(
        session: *mut SealedKernelBridgeSession,
        rel_path_utf8: *const c_char,
        limit: u64,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_query_chem_spectra_json(
        session: *mut SealedKernelBridgeSession,
        limit: u64,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_get_chem_spectra_default_limit(
        out_limit: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_get_chem_spectrum_json(
        session: *mut SealedKernelBridgeSession,
        attachment_rel_path_utf8: *const c_char,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_query_note_chem_spectrum_refs_json(
        session: *mut SealedKernelBridgeSession,
        note_rel_path_utf8: *const c_char,
        limit: u64,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_get_note_chem_spectrum_refs_default_limit(
        out_limit: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_query_chem_spectrum_referrers_json(
        session: *mut SealedKernelBridgeSession,
        attachment_rel_path_utf8: *const c_char,
        limit: u64,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_get_chem_spectrum_referrers_default_limit(
        out_limit: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_parse_spectroscopy_text_json(
        raw_utf8: *const c_char,
        raw_size: u64,
        extension_utf8: *const c_char,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_compute_truth_diff_json(
        prev_content: *const c_char,
        prev_size: u64,
        curr_content: *const c_char,
        curr_size: u64,
        file_extension_utf8: *const c_char,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_build_semantic_context_text(
        content: *const c_char,
        content_size: u64,
        out_text: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_get_semantic_context_min_bytes(
        out_bytes: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_get_rag_context_per_note_char_limit(
        out_chars: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_get_embedding_text_char_limit(
        out_chars: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_compute_truth_state_json(
        note_ids_utf8: *const *const c_char,
        active_secs: *const i64,
        activity_count: u64,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_compute_study_streak_days(
        day_buckets: *const i64,
        day_count: u64,
        today_bucket: i64,
        out_streak_days: *mut i64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_compute_study_stats_window(
        now_epoch_secs: i64,
        days_back: i64,
        out_today_start_epoch_secs: *mut i64,
        out_today_bucket: *mut i64,
        out_week_start_epoch_secs: *mut i64,
        out_daily_window_start_epoch_secs: *mut i64,
        out_heatmap_start_epoch_secs: *mut i64,
        out_folder_rank_limit: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_build_study_heatmap_grid_json(
        dates_utf8: *const *const c_char,
        active_secs: *const i64,
        day_count: u64,
        now_epoch_secs: i64,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_generate_mock_retrosynthesis_json(
        target_smiles_utf8: *const c_char,
        depth: u8,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_simulate_polymerization_kinetics_json(
        m0: f64,
        i0: f64,
        cta0: f64,
        kd: f64,
        kp: f64,
        kt: f64,
        ktr: f64,
        time_max: f64,
        steps: u64,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_recalculate_stoichiometry_json(
        mw: *const f64,
        eq: *const f64,
        moles: *const f64,
        mass: *const f64,
        volume: *const f64,
        density: *const f64,
        has_density: *const u8,
        is_reference: *const u8,
        count: u64,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_get_pdf_ink_default_tolerance(
        out_tolerance: *mut f32,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_compute_pdf_annotation_storage_key(
        pdf_path_utf8: *const c_char,
        out_key: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_compute_pdf_lightweight_hash(
        head: *const u8,
        head_size: u64,
        tail: *const u8,
        tail_size: u64,
        file_size: u64,
        out_hash: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_smooth_ink_strokes_json(
        xs: *const f32,
        ys: *const f32,
        pressures: *const f32,
        point_counts: *const u64,
        stroke_widths: *const f32,
        stroke_count: u64,
        tolerance: f32,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_build_molecular_preview_json(
        raw_utf8: *const c_char,
        raw_size: u64,
        extension_utf8: *const c_char,
        max_atoms: u64,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_normalize_molecular_preview_atom_limit(
        requested_atoms: u64,
        out_atoms: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_get_symmetry_atom_limit(
        out_atoms: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_get_crystal_supercell_atom_limit(
        out_atoms: *mut u64,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_calculate_symmetry_json(
        raw_utf8: *const c_char,
        raw_size: u64,
        format_utf8: *const c_char,
        max_atoms: u64,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_build_lattice_from_cif_json(
        raw_utf8: *const c_char,
        raw_size: u64,
        nx: u32,
        ny: u32,
        nz: u32,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_calculate_miller_plane_from_cif_json(
        raw_utf8: *const c_char,
        raw_size: u64,
        h: i32,
        k: i32,
        l: i32,
        out_json: *mut *mut c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_create_folder(
        session: *mut SealedKernelBridgeSession,
        folder_rel_path_utf8: *const c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_delete_entry(
        session: *mut SealedKernelBridgeSession,
        target_rel_path_utf8: *const c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_rename_entry(
        session: *mut SealedKernelBridgeSession,
        source_rel_path_utf8: *const c_char,
        new_name_utf8: *const c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
    fn sealed_kernel_bridge_move_entry(
        session: *mut SealedKernelBridgeSession,
        source_rel_path_utf8: *const c_char,
        dest_folder_rel_path_utf8: *const c_char,
        out_error: *mut *mut c_char,
    ) -> c_int;
}

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

#[derive(Deserialize, Serialize)]
struct SealedKernelNoteCatalog {
    notes: Vec<SealedKernelNoteRecord>,
}

#[derive(Deserialize, Serialize)]
struct SealedKernelNoteRecord {
    rel_path: String,
    title: String,
    #[serde(default)]
    mtime_ns: u64,
}

#[derive(Deserialize)]
struct SealedKernelTagCatalog {
    tags: Vec<SealedKernelTagRecord>,
}

#[derive(Deserialize)]
struct SealedKernelTagRecord {
    name: String,
    count: u32,
}

#[derive(Deserialize)]
struct SealedKernelReadNoteResult {
    content: String,
}

#[derive(Deserialize)]
struct SealedKernelFileTreeCatalog {
    nodes: Vec<SealedKernelFileTreeNode>,
}

#[derive(Deserialize)]
struct SealedKernelPathCatalog {
    paths: Vec<String>,
}

#[derive(Debug, Clone, Deserialize)]
pub struct SealedKernelTruthAward {
    pub attr: String,
    pub amount: i32,
    pub reason: i32,
    #[serde(default)]
    pub detail: String,
}

#[derive(Deserialize)]
struct SealedKernelTruthDiffResult {
    awards: Vec<SealedKernelTruthAward>,
}

#[derive(Debug, Clone, Deserialize)]
pub struct SealedKernelTruthAttributes {
    pub science: i64,
    pub engineering: i64,
    pub creation: i64,
    pub finance: i64,
}

#[derive(Debug, Clone, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct SealedKernelTruthState {
    pub level: i64,
    pub total_exp: i64,
    pub next_level_exp: i64,
    pub attributes: SealedKernelTruthAttributes,
    pub attribute_exp: SealedKernelTruthAttributes,
}

#[derive(Debug, Clone, Deserialize)]
pub struct SealedKernelHeatmapCell {
    pub date: String,
    pub secs: i64,
    pub col: usize,
    pub row: usize,
}

#[derive(Debug, Clone, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct SealedKernelHeatmapGrid {
    pub cells: Vec<SealedKernelHeatmapCell>,
    pub max_secs: i64,
}

#[derive(Debug, Clone, Copy)]
pub struct SealedKernelStudyStatsWindow {
    pub today_start_epoch_secs: i64,
    pub today_bucket: i64,
    pub week_start_epoch_secs: i64,
    pub daily_window_start_epoch_secs: i64,
    pub heatmap_start_epoch_secs: i64,
    pub folder_rank_limit: u64,
}

#[derive(Debug, Clone)]
pub struct SealedKernelStoichiometryInput {
    pub mw: f64,
    pub eq: f64,
    pub moles: f64,
    pub mass: f64,
    pub volume: f64,
    pub density: f64,
    pub has_density: bool,
    pub is_reference: bool,
}

#[derive(Debug, Clone, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct SealedKernelStoichiometryRow {
    pub mw: f64,
    pub eq: f64,
    pub moles: f64,
    pub mass: f64,
    pub volume: f64,
    pub density: f64,
    pub has_density: bool,
    pub is_reference: bool,
}

#[derive(Deserialize)]
struct SealedKernelStoichiometryResult {
    rows: Vec<SealedKernelStoichiometryRow>,
}

#[derive(Deserialize)]
#[serde(rename_all = "camelCase")]
struct SealedKernelFileTreeNode {
    name: String,
    full_name: String,
    relative_path: String,
    is_folder: bool,
    note: Option<SealedKernelFileTreeNote>,
    children: Vec<SealedKernelFileTreeNode>,
    file_count: u32,
}

#[derive(Deserialize)]
#[serde(rename_all = "camelCase")]
struct SealedKernelFileTreeNote {
    rel_path: String,
    name: String,
    extension: String,
    mtime_ns: u64,
}

fn take_bridge_string(ptr: *mut c_char) -> String {
    if ptr.is_null() {
        return String::new();
    }

    let value = unsafe { CStr::from_ptr(ptr).to_string_lossy().into_owned() };
    unsafe { sealed_kernel_bridge_free_string(ptr) };
    value
}

fn bridge_error(operation: &str, code: c_int, raw_error: *mut c_char) -> AppError {
    let message = take_bridge_string(raw_error);
    let detail = if message.is_empty() {
        format!("{operation} failed with kernel status {code}.")
    } else {
        format!("{operation} failed with kernel status {code}: {message}")
    };
    AppError::Custom(detail)
}

fn truth_diff_bridge_error(code: c_int, raw_error: *mut c_char) -> AppError {
    let token = take_bridge_string(raw_error);
    let message = match token.as_str() {
        "invalid_argument" => "Truth diff 内核参数无效".to_string(),
        "invalid_payload" => "Truth diff 内核返回结果无效".to_string(),
        "allocation_failed" => "Truth diff 内核结果分配失败".to_string(),
        "truth_diff_failed" | "" => "Truth diff 内核计算失败".to_string(),
        other => format!("Truth diff 内核计算失败 ({other}, status {code})"),
    };
    AppError::Custom(message)
}

fn semantic_context_bridge_error(code: c_int, raw_error: *mut c_char) -> AppError {
    let token = take_bridge_string(raw_error);
    let message = match token.as_str() {
        "invalid_argument" => "语义上下文内核参数无效".to_string(),
        "allocation_failed" => "语义上下文内核结果分配失败".to_string(),
        "semantic_context_failed" | "" => "语义上下文内核计算失败".to_string(),
        other => format!("语义上下文内核计算失败 ({other}, status {code})"),
    };
    AppError::Custom(message)
}

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

fn active_session(state: &SealedKernelState) -> AppResult<*mut SealedKernelBridgeSession> {
    let session = {
        let guard = state.session.lock().map_err(|_| AppError::Lock)?;
        *guard
    };
    session
        .map(|value| value as *mut SealedKernelBridgeSession)
        .ok_or_else(|| AppError::Custom("sealed kernel session is not open.".to_string()))
}

pub fn active_vault_path(state: &SealedKernelState) -> AppResult<String> {
    state
        .vault_path
        .lock()
        .map_err(|_| AppError::Lock)?
        .clone()
        .ok_or_else(|| AppError::Custom("sealed kernel vault is not open.".to_string()))
}

fn kernel_default_limit(
    operation: &str,
    getter: unsafe extern "C" fn(*mut u64, *mut *mut c_char) -> c_int,
) -> AppResult<u64> {
    let mut limit = 0u64;
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe { getter(&mut limit, &mut error) };
    if code != 0 {
        return Err(bridge_error(operation, code, error));
    }
    if limit == 0 {
        return Err(AppError::Custom(format!(
            "{operation} returned a zero default limit."
        )));
    }
    Ok(limit)
}

fn kernel_default_usize_limit(
    operation: &str,
    getter: unsafe extern "C" fn(*mut u64, *mut *mut c_char) -> c_int,
) -> AppResult<usize> {
    let value = kernel_default_limit(operation, getter)?;
    usize::try_from(value).map_err(|_| AppError::Custom(format!("{operation} exceeds host usize.")))
}

pub fn note_catalog_default_limit() -> AppResult<u64> {
    kernel_default_limit(
        "sealed_kernel_get_note_catalog_default_limit",
        sealed_kernel_bridge_get_note_catalog_default_limit,
    )
}

pub fn note_query_default_limit() -> AppResult<u64> {
    kernel_default_limit(
        "sealed_kernel_get_note_query_default_limit",
        sealed_kernel_bridge_get_note_query_default_limit,
    )
}

pub fn vault_scan_default_limit() -> AppResult<u64> {
    kernel_default_limit(
        "sealed_kernel_get_vault_scan_default_limit",
        sealed_kernel_bridge_get_vault_scan_default_limit,
    )
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
    let extension = Path::new(&rel_path)
        .extension()
        .and_then(|value| value.to_str())
        .unwrap_or("")
        .to_lowercase();
    let name = Path::new(&rel_path)
        .file_stem()
        .and_then(|value| value.to_str())
        .filter(|value| !value.is_empty())
        .unwrap_or(&record.title)
        .to_string();
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

pub fn query_note_infos(
    vault_path: &str,
    state: &SealedKernelState,
    limit: u64,
) -> AppResult<Vec<NoteInfo>> {
    ensure_vault_open(vault_path, state)?;
    wait_for_catalog_ready(active_session(state)?)?;
    let catalog = query_note_catalog(state, limit, None)?;
    Ok(catalog
        .notes
        .into_iter()
        .map(|record| note_info_from_record(vault_path, record))
        .collect())
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

pub fn file_tree_default_limit() -> AppResult<u64> {
    kernel_default_limit(
        "sealed_kernel_get_file_tree_default_limit",
        sealed_kernel_bridge_get_file_tree_default_limit,
    )
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

pub fn filter_changed_markdown_paths(paths: &[String]) -> AppResult<Vec<String>> {
    let joined = paths.join("\n");
    let paths_c = cstring_arg(joined, "changed_paths")?;
    let mut raw_json: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_filter_changed_markdown_paths_json(
            paths_c.as_ptr(),
            &mut raw_json,
            &mut error,
        )
    };
    if code != 0 {
        return Err(bridge_error(
            "sealed_kernel_filter_changed_markdown_paths",
            code,
            error,
        ));
    }

    let value = take_bridge_string(raw_json);
    let catalog: SealedKernelPathCatalog = serde_json::from_str(&value).map_err(|err| {
        AppError::Custom(format!("sealed kernel changed path JSON is invalid: {err}"))
    })?;
    Ok(catalog.paths)
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

pub fn search_note_default_limit() -> AppResult<u64> {
    kernel_default_limit(
        "sealed_kernel_get_search_note_default_limit",
        sealed_kernel_bridge_get_search_note_default_limit,
    )
}

pub fn backlink_default_limit() -> AppResult<u64> {
    kernel_default_limit(
        "sealed_kernel_get_backlink_default_limit",
        sealed_kernel_bridge_get_backlink_default_limit,
    )
}

pub fn tag_catalog_default_limit() -> AppResult<u64> {
    kernel_default_limit(
        "sealed_kernel_get_tag_catalog_default_limit",
        sealed_kernel_bridge_get_tag_catalog_default_limit,
    )
}

pub fn tag_note_default_limit() -> AppResult<u64> {
    kernel_default_limit(
        "sealed_kernel_get_tag_note_default_limit",
        sealed_kernel_bridge_get_tag_note_default_limit,
    )
}

pub fn tag_tree_default_limit() -> AppResult<u64> {
    kernel_default_limit(
        "sealed_kernel_get_tag_tree_default_limit",
        sealed_kernel_bridge_get_tag_tree_default_limit,
    )
}

pub fn graph_default_limit() -> AppResult<u64> {
    kernel_default_limit(
        "sealed_kernel_get_graph_default_limit",
        sealed_kernel_bridge_get_graph_default_limit,
    )
}

pub fn chem_spectra_default_limit() -> AppResult<u64> {
    kernel_default_limit(
        "sealed_kernel_get_chem_spectra_default_limit",
        sealed_kernel_bridge_get_chem_spectra_default_limit,
    )
}

pub fn note_chem_spectrum_refs_default_limit() -> AppResult<u64> {
    kernel_default_limit(
        "sealed_kernel_get_note_chem_spectrum_refs_default_limit",
        sealed_kernel_bridge_get_note_chem_spectrum_refs_default_limit,
    )
}

pub fn chem_spectrum_referrers_default_limit() -> AppResult<u64> {
    kernel_default_limit(
        "sealed_kernel_get_chem_spectrum_referrers_default_limit",
        sealed_kernel_bridge_get_chem_spectrum_referrers_default_limit,
    )
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
    let tags = query_tag_infos(state, limit)?;
    let mut root: Vec<TagTreeNode> = Vec::new();

    for tag in tags {
        let parts: Vec<&str> = tag
            .name
            .split('/')
            .filter(|part| !part.is_empty())
            .collect();
        if parts.is_empty() {
            continue;
        }

        let mut current_level = &mut root;
        for (index, segment) in parts.iter().enumerate() {
            let full_path = parts[..=index].join("/");
            let position = current_level.iter().position(|node| node.name == *segment);
            if let Some(position) = position {
                if full_path == tag.name {
                    current_level[position].count = tag.count;
                }
                current_level = &mut current_level[position].children;
            } else {
                let count = if full_path == tag.name { tag.count } else { 0 };
                current_level.push(TagTreeNode {
                    name: segment.to_string(),
                    full_path,
                    count,
                    children: Vec::new(),
                });
                let last = current_level.len() - 1;
                current_level = &mut current_level[last].children;
            }
        }
    }

    Ok(root)
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
    let GraphData { nodes, links } = query_graph_data(state, limit)?;
    let mut neighbors: HashMap<String, Vec<String>> = HashMap::with_capacity(nodes.len());
    let mut link_pairs = Vec::with_capacity(links.len() * 2);

    for link in &links {
        neighbors
            .entry(link.source.clone())
            .or_default()
            .push(link.target.clone());
        neighbors
            .entry(link.target.clone())
            .or_default()
            .push(link.source.clone());

        link_pairs.push(format!("{}->{}", link.source, link.target));
        link_pairs.push(format!("{}->{}", link.target, link.source));
    }

    Ok(EnrichedGraphData {
        nodes,
        links,
        neighbors,
        link_pairs,
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

fn spectroscopy_bridge_error(extension: &str, code: c_int, raw_error: *mut c_char) -> AppError {
    let token = take_bridge_string(raw_error);
    let message = match token.as_str() {
        "unsupported_extension" => format!("不支持的波谱文件扩展名: {extension}"),
        "csv_no_numeric_rows" => "CSV 中未找到有效的数值数据行".to_string(),
        "csv_too_few_columns" => "CSV 列数不足，至少需要 2 列".to_string(),
        "csv_no_valid_points" => "无法从 CSV 中提取有效数据点".to_string(),
        "jdx_no_points" => "JDX 文件中未找到可解析的数据点".to_string(),
        _ => format!("sealed_kernel_parse_spectroscopy_text failed with kernel status {code}."),
    };
    AppError::Custom(message)
}

fn molecular_preview_bridge_error(
    extension: &str,
    code: c_int,
    raw_error: *mut c_char,
) -> AppError {
    let token = take_bridge_string(raw_error);
    let message = match token.as_str() {
        "unsupported_extension" => format!("不支持的分子文件扩展名: {extension}"),
        _ => format!("sealed_kernel_build_molecular_preview failed with kernel status {code}."),
    };
    AppError::Custom(message)
}

fn retrosynthesis_bridge_error(code: c_int, raw_error: *mut c_char) -> AppError {
    let token = take_bridge_string(raw_error);
    let message = match token.as_str() {
        "invalid_argument" => "请输入目标分子 SMILES".to_string(),
        "empty_tree" | "invalid_payload" | "retro_failed" => "未生成可用逆合成路径".to_string(),
        _ => {
            format!("sealed_kernel_generate_mock_retrosynthesis failed with kernel status {code}.")
        }
    };
    AppError::Custom(message)
}

fn kinetics_bridge_error(code: c_int, raw_error: *mut c_char) -> AppError {
    let token = take_bridge_string(raw_error);
    let message = match token.as_str() {
        "invalid_argument" => "聚合动力学参数无效".to_string(),
        "empty_result" | "invalid_payload" | "kinetics_failed" => {
            "聚合动力学内核计算失败".to_string()
        }
        _ => format!("sealed bridge kinetics failed with kernel status {code}."),
    };
    AppError::Custom(message)
}

fn stoichiometry_bridge_error(code: c_int, raw_error: *mut c_char) -> AppError {
    let token = take_bridge_string(raw_error);
    let message = match token.as_str() {
        "invalid_argument" => "化学计量参数无效".to_string(),
        "allocation_failed" => "化学计量内核结果分配失败".to_string(),
        "stoichiometry_failed" | "" => "化学计量内核计算失败".to_string(),
        _ => format!("sealed bridge stoichiometry failed with kernel status {code}."),
    };
    AppError::Custom(message)
}

fn ink_smoothing_bridge_error(code: c_int, raw_error: *mut c_char) -> AppError {
    let token = take_bridge_string(raw_error);
    let message = match token.as_str() {
        "invalid_argument" => "笔迹平滑参数无效".to_string(),
        "invalid_payload" | "ink_smoothing_failed" => "笔迹平滑内核计算失败".to_string(),
        _ => format!("sealed bridge ink smoothing failed with kernel status {code}."),
    };
    AppError::Custom(message)
}

fn symmetry_bridge_error(
    format: &str,
    max_atoms: usize,
    code: c_int,
    raw_error: *mut c_char,
) -> AppError {
    let token = take_bridge_string(raw_error);
    let message = match token.as_str() {
        "parse_unsupported_format" => format!("不支持的分子文件格式: {format}"),
        "parse_xyz_empty" => "XYZ 文件为空".to_string(),
        "parse_xyz_incomplete" => "XYZ 文件格式不完整".to_string(),
        "parse_xyz_coordinate" => "XYZ 坐标解析失败".to_string(),
        "parse_pdb_coordinate" => "PDB 坐标解析失败".to_string(),
        "parse_cif_missing_cell" => {
            "CIF 使用分数坐标，但缺少完整晶胞参数 (_cell_length_*/_cell_angle_*)".to_string()
        }
        "parse_cif_invalid_cell" => "CIF 晶胞参数非法：无法构造有效的晶胞基矢".to_string(),
        "no_atoms" => "未找到任何原子坐标".to_string(),
        token if token.starts_with("too_many_atoms:") => {
            let atom_count = token
                .split_once(':')
                .and_then(|(_, value)| value.parse::<usize>().ok())
                .unwrap_or(0);
            format!("原子数 ({atom_count}) 超过对称性分析上限 ({max_atoms})，请使用较小的分子")
        }
        _ => format!("sealed_kernel_calculate_symmetry failed with kernel status {code}."),
    };
    AppError::Custom(message)
}

fn atom_limit_bridge_error(surface: &str, code: c_int, raw_error: *mut c_char) -> AppError {
    let token = take_bridge_string(raw_error);
    let detail = if token.is_empty() {
        "unknown".to_string()
    } else {
        token
    };
    AppError::Custom(format!(
        "sealed kernel {surface} atom limit query failed with {detail} ({code})."
    ))
}

fn crystal_parse_error_message(token: &str) -> Option<String> {
    match token {
        "parse_missing_cell" => {
            Some("CIF 文件缺少完整的晶胞参数 (_cell_length_*/_cell_angle_*)".to_string())
        }
        "parse_missing_atoms" => {
            Some("CIF 文件中未找到分数坐标原子 (_atom_site_fract_*)".to_string())
        }
        _ => None,
    }
}

fn crystal_lattice_bridge_error(code: c_int, raw_error: *mut c_char) -> AppError {
    let token = take_bridge_string(raw_error);
    let message = crystal_parse_error_message(&token).unwrap_or_else(|| match token.as_str() {
        "supercell_gamma_too_small" => "晶胞参数非法：gamma 角过小".to_string(),
        "supercell_invalid_basis" => "晶胞参数非法：无法构造有效基矢".to_string(),
        token if token.starts_with("supercell_too_many_atoms:") => {
            let estimated_count = token
                .split_once(':')
                .and_then(|(_, value)| value.parse::<u64>().ok())
                .unwrap_or(0);
            match crystal_supercell_atom_limit() {
                Ok(limit) => {
                    format!("超晶胞原子数 ({estimated_count}) 超过上限 ({limit})，请减小扩展维度")
                }
                Err(_) => {
                    format!("超晶胞原子数 ({estimated_count}) 超过 kernel 上限，请减小扩展维度")
                }
            }
        }
        _ => format!("sealed_kernel_build_lattice_from_cif failed with kernel status {code}."),
    });
    AppError::Custom(message)
}

fn crystal_miller_bridge_error(code: c_int, raw_error: *mut c_char) -> AppError {
    let token = take_bridge_string(raw_error);
    let message = crystal_parse_error_message(&token).unwrap_or_else(|| match token.as_str() {
        "miller_zero_index" => "密勒指数 (h, k, l) 不能全为零".to_string(),
        "miller_gamma_too_small" => "晶胞参数非法：gamma 角过小".to_string(),
        "miller_invalid_basis" => "晶胞参数非法：无法构造有效基矢".to_string(),
        "miller_zero_volume" => "晶胞体积为零，无法计算密勒面".to_string(),
        "miller_zero_normal" => "法向量长度为零".to_string(),
        _ => {
            format!(
                "sealed_kernel_calculate_miller_plane_from_cif failed with kernel status {code}."
            )
        }
    });
    AppError::Custom(message)
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

pub fn semantic_context_min_bytes() -> AppResult<usize> {
    kernel_default_usize_limit(
        "sealed_kernel_get_semantic_context_min_bytes",
        sealed_kernel_bridge_get_semantic_context_min_bytes,
    )
}

pub fn rag_context_per_note_char_limit() -> AppResult<usize> {
    kernel_default_usize_limit(
        "sealed_kernel_get_rag_context_per_note_char_limit",
        sealed_kernel_bridge_get_rag_context_per_note_char_limit,
    )
}

pub fn embedding_text_char_limit() -> AppResult<usize> {
    kernel_default_usize_limit(
        "sealed_kernel_get_embedding_text_char_limit",
        sealed_kernel_bridge_get_embedding_text_char_limit,
    )
}

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

pub fn compute_study_streak_days(day_buckets: &[i64], today_bucket: i64) -> AppResult<i64> {
    let mut streak_days = 0;
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_compute_study_streak_days(
            day_buckets.as_ptr(),
            day_buckets.len() as u64,
            today_bucket,
            &mut streak_days,
            &mut error,
        )
    };
    if code != 0 {
        return Err(bridge_error(
            "sealed_kernel_compute_study_streak_days",
            code,
            error,
        ));
    }
    Ok(streak_days)
}

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

pub fn pdf_ink_default_tolerance() -> AppResult<f32> {
    let mut tolerance = 0.0f32;
    let mut error: *mut c_char = std::ptr::null_mut();
    let code =
        unsafe { sealed_kernel_bridge_get_pdf_ink_default_tolerance(&mut tolerance, &mut error) };
    if code != 0 {
        return Err(bridge_error(
            "sealed_kernel_get_pdf_ink_default_tolerance",
            code,
            error,
        ));
    }
    if !tolerance.is_finite() || tolerance <= 0.0 {
        return Err(AppError::Custom(
            "kernel PDF ink default tolerance must be positive.".to_string(),
        ));
    }
    Ok(tolerance)
}

pub fn pdf_annotation_storage_key(pdf_path: &str) -> AppResult<String> {
    let pdf_path_c = cstring_arg(pdf_path.to_string(), "pdf_path")?;
    let mut raw_key: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_compute_pdf_annotation_storage_key(
            pdf_path_c.as_ptr(),
            &mut raw_key,
            &mut error,
        )
    };
    if code != 0 {
        return Err(bridge_error(
            "sealed_kernel_compute_pdf_annotation_storage_key",
            code,
            error,
        ));
    }
    Ok(take_bridge_string(raw_key))
}

pub fn compute_pdf_lightweight_hash(head: &[u8], tail: &[u8], file_size: u64) -> AppResult<String> {
    let head_ptr = if head.is_empty() {
        std::ptr::null()
    } else {
        head.as_ptr()
    };
    let tail_ptr = if tail.is_empty() {
        std::ptr::null()
    } else {
        tail.as_ptr()
    };

    let mut raw_hash: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_compute_pdf_lightweight_hash(
            head_ptr,
            head.len() as u64,
            tail_ptr,
            tail.len() as u64,
            file_size,
            &mut raw_hash,
            &mut error,
        )
    };
    if code != 0 {
        return Err(bridge_error(
            "sealed_kernel_compute_pdf_lightweight_hash",
            code,
            error,
        ));
    }
    Ok(take_bridge_string(raw_hash))
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

pub fn normalize_molecular_preview_atom_limit(requested_atoms: usize) -> AppResult<usize> {
    let mut normalized = 0u64;
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_normalize_molecular_preview_atom_limit(
            requested_atoms as u64,
            &mut normalized,
            &mut error,
        )
    };
    if code != 0 {
        return Err(molecular_preview_bridge_error("", code, error));
    }
    Ok(normalized as usize)
}

fn symmetry_atom_limit() -> AppResult<usize> {
    let mut limit = 0u64;
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe { sealed_kernel_bridge_get_symmetry_atom_limit(&mut limit, &mut error) };
    if code != 0 {
        return Err(atom_limit_bridge_error("symmetry", code, error));
    }
    usize::try_from(limit)
        .map_err(|_| AppError::Custom("kernel symmetry atom limit exceeds host usize".to_string()))
}

fn crystal_supercell_atom_limit() -> AppResult<usize> {
    let mut limit = 0u64;
    let mut error: *mut c_char = std::ptr::null_mut();
    let code =
        unsafe { sealed_kernel_bridge_get_crystal_supercell_atom_limit(&mut limit, &mut error) };
    if code != 0 {
        return Err(atom_limit_bridge_error("crystal supercell", code, error));
    }
    usize::try_from(limit).map_err(|_| {
        AppError::Custom("kernel crystal supercell atom limit exceeds host usize".to_string())
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
        return Err(crystal_lattice_bridge_error(code, error));
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

fn rel_path_from_file_path(vault_path: &str, file_path: &str) -> AppResult<String> {
    let vault = PathBuf::from(vault_path);
    let target = PathBuf::from(file_path);
    let rel = target
        .strip_prefix(&vault)
        .map_err(|_| AppError::Custom(format!("文件不在当前 vault 内: {file_path}")))?;
    let rel_path = rel.to_string_lossy().replace('\\', "/");
    if rel_path.trim().is_empty() || rel_path.contains("..") {
        return Err(AppError::Custom("非法笔记路径".to_string()));
    }
    Ok(rel_path)
}

fn validate_rel_path(rel_path: &str, label: &str) -> AppResult<String> {
    let normalized = rel_path.trim().replace('\\', "/");
    if normalized.is_empty()
        || normalized.starts_with('/')
        || normalized.contains('\0')
        || normalized
            .split('/')
            .any(|part| part.is_empty() || part == "." || part == "..")
        || Path::new(&normalized).is_absolute()
    {
        return Err(AppError::Custom(format!("非法{label}路径")));
    }
    Ok(normalized)
}

fn rel_path_from_optional_folder_path(vault_path: &str, folder_path: &str) -> AppResult<String> {
    let vault = PathBuf::from(vault_path);
    let target = PathBuf::from(folder_path);
    let rel = target
        .strip_prefix(&vault)
        .map_err(|_| AppError::Custom(format!("文件夹不在当前 vault 内: {folder_path}")))?;
    let rel_path = rel.to_string_lossy().replace('\\', "/");
    if rel_path.contains("..") {
        return Err(AppError::Custom("非法文件夹路径".to_string()));
    }
    Ok(rel_path)
}

fn cstring_arg(value: String, label: &str) -> AppResult<CString> {
    CString::new(value)
        .map_err(|_| AppError::Custom(format!("{label} must not contain NUL bytes.")))
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

pub fn read_note_by_file_path(file_path: &str, state: &SealedKernelState) -> AppResult<String> {
    let vault_path = active_vault_path(state)?;
    let rel_path = rel_path_from_file_path(&vault_path, file_path)?;
    read_note_by_rel_path(&rel_path, state)
}

pub fn read_note_by_rel_path(rel_path: &str, state: &SealedKernelState) -> AppResult<String> {
    let rel_path = validate_rel_path(rel_path, "笔记")?;
    let rel_path_c = cstring_arg(rel_path, "rel_path")?;
    let session = active_session(state)?;

    let mut raw_json: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_read_note_json(session, rel_path_c.as_ptr(), &mut raw_json, &mut error)
    };
    if code != 0 {
        return Err(bridge_error("sealed_kernel_read_note", code, error));
    }

    let value = take_bridge_string(raw_json);
    let parsed: SealedKernelReadNoteResult = serde_json::from_str(&value).map_err(|err| {
        AppError::Custom(format!("sealed kernel note read JSON is invalid: {err}"))
    })?;
    Ok(parsed.content)
}

pub fn write_note_by_file_path(
    vault_path: &str,
    file_path: &str,
    content: &str,
    state: &SealedKernelState,
) -> AppResult<()> {
    ensure_vault_open(vault_path, state)?;
    let rel_path = rel_path_from_file_path(vault_path, file_path)?;
    let rel_path_c = CString::new(rel_path)
        .map_err(|_| AppError::Custom("rel_path must not contain NUL bytes.".to_string()))?;
    let content_c = CString::new(content)
        .map_err(|_| AppError::Custom("content must not contain NUL bytes.".to_string()))?;
    let session = active_session(state)?;

    let mut raw_json: *mut c_char = std::ptr::null_mut();
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe {
        sealed_kernel_bridge_write_note_json(
            session,
            rel_path_c.as_ptr(),
            content_c.as_ptr(),
            content.len() as u64,
            &mut raw_json,
            &mut error,
        )
    };
    if code != 0 {
        return Err(bridge_error("sealed_kernel_write_note", code, error));
    }

    let _ = take_bridge_string(raw_json);
    Ok(())
}

pub fn create_folder_by_path(
    vault_path: &str,
    folder_path: &str,
    state: &SealedKernelState,
) -> AppResult<()> {
    ensure_vault_open(vault_path, state)?;
    let rel_path = rel_path_from_file_path(vault_path, folder_path)?;
    let rel_path_c = cstring_arg(rel_path, "folder_rel_path")?;
    let session = active_session(state)?;
    call_status_operation("sealed_kernel_create_folder", |error| unsafe {
        sealed_kernel_bridge_create_folder(session, rel_path_c.as_ptr(), error)
    })
}

pub fn delete_entry_by_path(
    vault_path: &str,
    target_path: &str,
    state: &SealedKernelState,
) -> AppResult<()> {
    ensure_vault_open(vault_path, state)?;
    let rel_path = rel_path_from_file_path(vault_path, target_path)?;
    let rel_path_c = cstring_arg(rel_path, "target_rel_path")?;
    let session = active_session(state)?;
    call_status_operation("sealed_kernel_delete_entry", |error| unsafe {
        sealed_kernel_bridge_delete_entry(session, rel_path_c.as_ptr(), error)
    })
}

pub fn rename_entry_by_path(
    vault_path: &str,
    source_path: &str,
    new_name: &str,
    state: &SealedKernelState,
) -> AppResult<()> {
    ensure_vault_open(vault_path, state)?;
    let rel_path = rel_path_from_file_path(vault_path, source_path)?;
    let rel_path_c = cstring_arg(rel_path, "source_rel_path")?;
    let new_name_c = cstring_arg(new_name.to_string(), "new_name")?;
    let session = active_session(state)?;
    call_status_operation("sealed_kernel_rename_entry", |error| unsafe {
        sealed_kernel_bridge_rename_entry(session, rel_path_c.as_ptr(), new_name_c.as_ptr(), error)
    })
}

pub fn move_entry_by_path(
    vault_path: &str,
    source_path: &str,
    dest_folder: &str,
    state: &SealedKernelState,
) -> AppResult<()> {
    ensure_vault_open(vault_path, state)?;
    let source_rel_path = rel_path_from_file_path(vault_path, source_path)?;
    let dest_rel_path = rel_path_from_optional_folder_path(vault_path, dest_folder)?;
    let source_rel_path_c = cstring_arg(source_rel_path, "source_rel_path")?;
    let dest_rel_path_c = if dest_rel_path.is_empty() {
        None
    } else {
        Some(cstring_arg(dest_rel_path, "dest_folder_rel_path")?)
    };
    let dest_ptr = dest_rel_path_c
        .as_ref()
        .map_or(std::ptr::null(), |value| value.as_ptr());
    let session = active_session(state)?;
    call_status_operation("sealed_kernel_move_entry", |error| unsafe {
        sealed_kernel_bridge_move_entry(session, source_rel_path_c.as_ptr(), dest_ptr, error)
    })
}

#[tauri::command]
pub fn sealed_kernel_bridge_info() -> AppResult<Value> {
    let raw = unsafe { sealed_kernel_bridge_info_json() };
    let value = take_bridge_string(raw);
    serde_json::from_str(&value).map_err(|err| {
        AppError::Custom(format!(
            "sealed kernel bridge info is not valid JSON: {err}"
        ))
    })
}

#[tauri::command]
pub fn sealed_kernel_open_vault(
    vault_path: String,
    state: State<'_, SealedKernelState>,
) -> AppResult<SealedKernelSessionSummary> {
    open_vault_inner(vault_path, state.inner())
}

#[tauri::command]
pub fn sealed_kernel_get_state(
    state: State<'_, SealedKernelState>,
) -> AppResult<SealedKernelSessionSummary> {
    let session = {
        let guard = state.session.lock().map_err(|_| AppError::Lock)?;
        *guard
    };
    let vault_path = state.vault_path.lock().map_err(|_| AppError::Lock)?.clone();

    let Some(session) = session else {
        return Ok(SealedKernelSessionSummary {
            active: false,
            vault_path: None,
            state: None,
        });
    };

    let state_snapshot = get_state_for_session(session as *mut SealedKernelBridgeSession)?;
    Ok(SealedKernelSessionSummary {
        active: true,
        vault_path,
        state: Some(state_snapshot),
    })
}

#[tauri::command]
pub fn sealed_kernel_close_vault(
    state: State<'_, SealedKernelState>,
) -> AppResult<SealedKernelSessionSummary> {
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

    Ok(SealedKernelSessionSummary {
        active: false,
        vault_path: None,
        state: None,
    })
}

#[tauri::command]
pub fn sealed_kernel_query_notes(
    limit: Option<u64>,
    state: State<'_, SealedKernelState>,
) -> AppResult<Value> {
    let limit = match limit {
        Some(value) => value,
        None => note_query_default_limit()?,
    };
    serde_json::to_value(query_note_catalog(state.inner(), limit, None)?)
        .map_err(|err| AppError::Custom(format!("sealed kernel note catalog encode failed: {err}")))
}

#[tauri::command]
pub fn sealed_kernel_query_chem_spectra(
    limit: Option<u64>,
    state: State<'_, SealedKernelState>,
) -> AppResult<Value> {
    let limit = match limit {
        Some(value) => value,
        None => chem_spectra_default_limit()?,
    };
    query_chem_spectra_value(state.inner(), limit)
}

#[tauri::command]
pub fn sealed_kernel_get_chem_spectrum(
    attachment_rel_path: String,
    state: State<'_, SealedKernelState>,
) -> AppResult<Value> {
    get_chem_spectrum_value(state.inner(), &attachment_rel_path)
}

#[tauri::command]
pub fn sealed_kernel_query_note_chem_spectrum_refs(
    note_rel_path: String,
    limit: Option<u64>,
    state: State<'_, SealedKernelState>,
) -> AppResult<Value> {
    let limit = match limit {
        Some(value) => value,
        None => note_chem_spectrum_refs_default_limit()?,
    };
    query_note_chem_spectrum_refs_value(state.inner(), &note_rel_path, limit)
}

#[tauri::command]
pub fn sealed_kernel_query_chem_spectrum_referrers(
    attachment_rel_path: String,
    limit: Option<u64>,
    state: State<'_, SealedKernelState>,
) -> AppResult<Value> {
    let limit = match limit {
        Some(value) => value,
        None => chem_spectrum_referrers_default_limit()?,
    };
    query_chem_spectrum_referrers_value(state.inner(), &attachment_rel_path, limit)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn validate_rel_path_normalizes_windows_separators() {
        let value = validate_rel_path("folder\\note.md", "笔记").expect("valid rel path");
        assert_eq!(value, "folder/note.md");
    }

    #[test]
    fn validate_rel_path_rejects_parent_segments() {
        assert!(validate_rel_path("folder/../note.md", "笔记").is_err());
    }

    #[test]
    fn validate_rel_path_rejects_rooted_paths() {
        assert!(validate_rel_path("/note.md", "笔记").is_err());
        assert!(validate_rel_path("C:/vault/note.md", "笔记").is_err());
    }

    #[test]
    fn filter_supported_vault_paths_filtered_uses_kernel_hidden_and_ignored_rules() {
        let paths = vec![
            " Folder\\Note.md ".to_string(),
            "Folder/Note.md".to_string(),
            ".hidden/Note.md".to_string(),
            "Folder/.hidden/Note.md".to_string(),
            "node_modules/Note.md".to_string(),
            "Other.PDF".to_string(),
            "Other.exe".to_string(),
        ];

        assert_eq!(
            filter_supported_vault_paths_filtered(&paths, " node_modules ")
                .expect("kernel filtered supported path filter"),
            vec!["Folder/Note.md".to_string(), "Other.PDF".to_string()]
        );
    }

    #[test]
    fn compute_truth_diff_uses_sealed_bridge_code_language_award() {
        let result = compute_truth_diff("note", "note\n```rust\nfn main() {}\n```", "md")
            .expect("sealed bridge truth diff");

        assert_eq!(result.len(), 1);
        assert_eq!(result[0].attr, "engineering");
        assert_eq!(result[0].amount, 8);
        assert_eq!(result[0].reason, 2);
        assert_eq!(result[0].detail, "rust");
    }

    #[test]
    fn build_semantic_context_uses_sealed_bridge_short_trim() {
        let result =
            build_semantic_context("  short note  \n").expect("sealed bridge semantic context");

        assert_eq!(result, "short note");
    }

    #[test]
    fn parse_spectroscopy_from_text_uses_sealed_bridge_csv_parser() {
        let parsed = parse_spectroscopy_from_text(
            "ppm,intensity,fit\n1.0,5.0,4.5\n2.0,6.0,bad\n3.0,7.0\n",
            "csv",
        )
        .expect("sealed bridge csv parse");

        assert_eq!(parsed.x, vec![1.0, 2.0, 3.0]);
        assert_eq!(parsed.x_label, "ppm");
        assert!(parsed.is_nmr);
        assert_eq!(parsed.series.len(), 2);
        assert_eq!(parsed.series[0].label, "intensity");
        assert_eq!(parsed.series[0].y, vec![5.0, 6.0, 7.0]);
        assert_eq!(parsed.series[1].label, "fit");
        assert_eq!(parsed.series[1].y, vec![4.5, 0.0, 0.0]);
    }

    #[test]
    fn parse_spectroscopy_from_text_uses_sealed_bridge_jdx_parser() {
        let parsed = parse_spectroscopy_from_text(
            "##TITLE=Sample NMR\n\
             ##DATATYPE=NMR SPECTRUM\n\
             ##XUNITS=PPM\n\
             ##YUNITS=INTENSITY\n\
             ##PEAK TABLE=(XY..XY)\n\
             1.0, 10.0; 2.0, 11.0\n\
             ##END=\n",
            "jdx",
        )
        .expect("sealed bridge jdx parse");

        assert_eq!(parsed.title, "Sample NMR");
        assert_eq!(parsed.x_label, "PPM");
        assert!(parsed.is_nmr);
        assert_eq!(parsed.x, vec![1.0, 2.0]);
        assert_eq!(parsed.series.len(), 1);
        assert_eq!(parsed.series[0].label, "INTENSITY");
        assert_eq!(parsed.series[0].y, vec![10.0, 11.0]);
    }

    #[test]
    fn normalize_molecular_preview_atom_limit_uses_kernel_bounds() {
        assert_eq!(normalize_molecular_preview_atom_limit(0).unwrap(), 2000);
        assert_eq!(normalize_molecular_preview_atom_limit(2).unwrap(), 200);
        assert_eq!(normalize_molecular_preview_atom_limit(500).unwrap(), 500);
        assert_eq!(
            normalize_molecular_preview_atom_limit(50000).unwrap(),
            20000
        );
    }

    #[test]
    fn compute_atom_limits_come_from_kernel() {
        assert_eq!(symmetry_atom_limit().unwrap(), 500);
        assert_eq!(crystal_supercell_atom_limit().unwrap(), 50000);
    }

    #[test]
    fn note_catalog_default_limit_comes_from_kernel() {
        assert_eq!(note_catalog_default_limit().unwrap(), 100000);
    }

    #[test]
    fn note_query_default_limit_comes_from_kernel() {
        assert_eq!(note_query_default_limit().unwrap(), 512);
    }

    #[test]
    fn product_text_limits_come_from_kernel() {
        assert_eq!(semantic_context_min_bytes().unwrap(), 24);
        assert_eq!(rag_context_per_note_char_limit().unwrap(), 1500);
        assert_eq!(embedding_text_char_limit().unwrap(), 2000);
    }

    #[test]
    fn compute_truth_state_uses_kernel_activity_rules() {
        let state = compute_truth_state_from_activity(&[
            ("lab.csv".to_string(), 120),
            ("code.rs".to_string(), 3600),
            ("molecule.mol".to_string(), 3000),
            ("ledger.base".to_string(), 6000),
        ])
        .unwrap();

        assert_eq!(state.level, 2);
        assert_eq!(state.total_exp, 112);
        assert_eq!(state.next_level_exp, 150);
        assert_eq!(state.attribute_exp.science, 2);
        assert_eq!(state.attribute_exp.engineering, 60);
        assert_eq!(state.attribute_exp.creation, 50);
        assert_eq!(state.attribute_exp.finance, 100);
        assert_eq!(state.attributes.science, 1);
        assert_eq!(state.attributes.engineering, 2);
        assert_eq!(state.attributes.creation, 2);
        assert_eq!(state.attributes.finance, 3);
    }

    #[test]
    fn compute_study_streak_uses_kernel_contiguous_day_rules() {
        assert_eq!(
            compute_study_streak_days(&[12, 10, 9, 10, 8, 2], 10).unwrap(),
            3
        );
        assert_eq!(
            compute_study_streak_days(&[12, 10, 9, 10, 8, 2], 11).unwrap(),
            0
        );
    }

    #[test]
    fn compute_study_stats_window_uses_kernel_calendar_rules() {
        let window = compute_study_stats_window(1714305600, 7).unwrap();

        assert_eq!(window.today_start_epoch_secs, 1714262400);
        assert_eq!(window.today_bucket, 19841);
        assert_eq!(window.week_start_epoch_secs, 1713744000);
        assert_eq!(window.daily_window_start_epoch_secs, 1713744000);
        assert_eq!(window.heatmap_start_epoch_secs, 1698796800);
        assert_eq!(window.folder_rank_limit, 5);
    }

    #[test]
    fn build_study_heatmap_grid_uses_kernel_calendar_rules() {
        let grid = build_study_heatmap_grid(
            &[
                ("2023-10-30".to_string(), 60),
                ("2024-01-01".to_string(), 120),
                ("2024-01-01".to_string(), 30),
                ("2024-04-28".to_string(), 300),
                ("2022-01-01".to_string(), 999),
            ],
            1714305600,
        )
        .unwrap();

        assert_eq!(grid.cells.len(), 182);
        assert_eq!(grid.max_secs, 300);
        assert_eq!(grid.cells[0].date, "2023-10-30");
        assert_eq!(grid.cells[0].secs, 60);
        assert_eq!(grid.cells[0].col, 0);
        assert_eq!(grid.cells[0].row, 0);
        assert_eq!(grid.cells[63].date, "2024-01-01");
        assert_eq!(grid.cells[63].secs, 150);
        assert_eq!(grid.cells[181].date, "2024-04-28");
        assert_eq!(grid.cells[181].secs, 300);
        assert_eq!(grid.cells[181].col, 25);
        assert_eq!(grid.cells[181].row, 6);
    }

    #[test]
    fn vault_scan_default_limit_comes_from_kernel() {
        assert_eq!(vault_scan_default_limit().unwrap(), 4096);
    }

    #[test]
    fn file_tree_default_limit_comes_from_kernel() {
        assert_eq!(file_tree_default_limit().unwrap(), 4096);
    }

    #[test]
    fn relationship_default_limits_come_from_kernel() {
        assert_eq!(search_note_default_limit().unwrap(), 10);
        assert_eq!(backlink_default_limit().unwrap(), 64);
        assert_eq!(tag_catalog_default_limit().unwrap(), 512);
        assert_eq!(tag_note_default_limit().unwrap(), 128);
        assert_eq!(tag_tree_default_limit().unwrap(), 512);
        assert_eq!(graph_default_limit().unwrap(), 2048);
    }

    #[test]
    fn chemistry_spectrum_default_limits_come_from_kernel() {
        assert_eq!(chem_spectra_default_limit().unwrap(), 512);
        assert_eq!(note_chem_spectrum_refs_default_limit().unwrap(), 512);
        assert_eq!(chem_spectrum_referrers_default_limit().unwrap(), 512);
    }

    #[test]
    fn parse_spectroscopy_from_text_maps_sealed_bridge_parse_errors() {
        let err = parse_spectroscopy_from_text("name,value\nnot-a-number,still-bad\n", "csv")
            .expect_err("invalid csv");
        assert_eq!(err.to_string(), "CSV 中未找到有效的数值数据行");

        let err = parse_spectroscopy_from_text("1,2\n", "txt").expect_err("unsupported");
        assert_eq!(err.to_string(), "不支持的波谱文件扩展名: txt");
    }

    #[test]
    fn generate_mock_retrosynthesis_uses_sealed_bridge_amide_rules() {
        let result = generate_mock_retrosynthesis(" CC(=O)NCC1=CC=CC=C1 ", 2)
            .expect("sealed bridge retrosynthesis");

        assert!(!result.pathways.is_empty());
        assert_eq!(result.pathways[0].reaction_name, "Amide Coupling");
        assert!(result.pathways[0].target_id.starts_with("retro_"));
        assert_eq!(result.pathways[0].precursors[2].role, "reagent");
    }

    #[test]
    fn generate_mock_retrosynthesis_maps_sealed_bridge_invalid_argument() {
        let err = generate_mock_retrosynthesis("   ", 2).expect_err("empty target");

        assert_eq!(err.to_string(), "请输入目标分子 SMILES");
    }

    fn default_kinetics_params() -> KineticsParams {
        KineticsParams {
            m0: 1.0,
            i0: 0.01,
            cta0: 0.001,
            kd: 0.001,
            kp: 100.0,
            kt: 1000.0,
            ktr: 0.1,
            time_max: 3600.0,
            steps: 120,
        }
    }

    #[test]
    fn simulate_polymerization_kinetics_uses_sealed_bridge_series() {
        let params = default_kinetics_params();
        let result =
            simulate_polymerization_kinetics(params.clone()).expect("sealed bridge kinetics");

        assert_eq!(result.time.len(), params.steps + 1);
        assert_eq!(result.conversion.len(), result.time.len());
        assert_eq!(result.mn.len(), result.time.len());
        assert_eq!(result.pdi.len(), result.time.len());
        assert_eq!(result.time[0], 0.0);
        assert!((result.time[result.time.len() - 1] - params.time_max).abs() < 1.0e-9);
        assert!(result
            .pdi
            .iter()
            .all(|value| value.is_finite() && *value >= 1.0));
    }

    #[test]
    fn simulate_polymerization_kinetics_maps_invalid_argument() {
        let mut params = default_kinetics_params();
        params.m0 = 0.0;

        let err = simulate_polymerization_kinetics(params).expect_err("invalid kinetics params");

        assert_eq!(err.to_string(), "聚合动力学参数无效");
    }

    fn stoichiometry_input(
        mw: f64,
        eq: f64,
        moles: f64,
        mass: f64,
        volume: f64,
        is_reference: bool,
        density: Option<f64>,
    ) -> SealedKernelStoichiometryInput {
        SealedKernelStoichiometryInput {
            mw,
            eq,
            moles,
            mass,
            volume,
            density: density.unwrap_or(0.0),
            has_density: density.is_some(),
            is_reference,
        }
    }

    #[test]
    fn recalculate_stoichiometry_uses_sealed_bridge_reference_row() {
        let result = recalculate_stoichiometry(&[
            stoichiometry_input(50.0, 2.0, 99.0, 0.0, 0.0, false, None),
            stoichiometry_input(100.0, 7.0, 0.25, 9.0, 3.0, true, Some(2.0)),
            stoichiometry_input(10.0, 3.0, 99.0, 8.0, 4.0, true, None),
        ])
        .expect("sealed bridge stoichiometry");

        assert_eq!(result.len(), 3);
        assert!(!result[0].is_reference);
        assert!(result[1].is_reference);
        assert!(!result[2].is_reference);
        assert_eq!(result[1].eq, 1.0);
        assert_eq!(result[1].moles, 0.25);
        assert_eq!(result[1].mass, 25.0);
        assert!(result[1].has_density);
        assert_eq!(result[1].density, 2.0);
        assert_eq!(result[1].volume, 12.5);
        assert_eq!(result[2].eq, 3.0);
        assert_eq!(result[2].moles, 0.75);
        assert_eq!(result[2].mass, 7.5);
        assert!(result[2].has_density);
        assert_eq!(result[2].density, 2.0);
        assert_eq!(result[2].volume, 3.75);
    }

    #[test]
    fn recalculate_stoichiometry_delegates_empty_rows_through_sealed_bridge() {
        let result = recalculate_stoichiometry(&[]).expect("sealed bridge empty stoichiometry");

        assert!(result.is_empty());
    }

    #[test]
    fn smooth_ink_strokes_uses_sealed_bridge_series() {
        let strokes = vec![RawStroke {
            points: vec![
                crate::pdf::annotations::InkPoint {
                    x: 0.0,
                    y: 0.0,
                    pressure: 0.5,
                },
                crate::pdf::annotations::InkPoint {
                    x: 0.5,
                    y: 0.2,
                    pressure: 0.7,
                },
                crate::pdf::annotations::InkPoint {
                    x: 1.0,
                    y: 0.0,
                    pressure: 0.9,
                },
            ],
            stroke_width: 0.01,
        }];

        let smoothed = smooth_ink_strokes(strokes, 0.001).expect("sealed bridge ink smoothing");

        assert_eq!(smoothed.len(), 1);
        assert!(smoothed[0].points.len() > 3);
        assert_eq!(smoothed[0].stroke_width, 0.01);
        assert!((smoothed[0].points[0].x - 0.0).abs() < 1.0e-6);
        assert!((smoothed[0].points.last().unwrap().x - 1.0).abs() < 1.0e-6);
    }

    #[test]
    fn smooth_ink_strokes_preserves_two_point_strokes_through_sealed_bridge() {
        let strokes = vec![RawStroke {
            points: vec![
                crate::pdf::annotations::InkPoint {
                    x: 0.0,
                    y: 0.0,
                    pressure: 0.5,
                },
                crate::pdf::annotations::InkPoint {
                    x: 1.0,
                    y: 1.0,
                    pressure: 0.8,
                },
            ],
            stroke_width: 0.02,
        }];

        let smoothed = smooth_ink_strokes(strokes, 0.1).expect("sealed bridge ink smoothing");

        assert_eq!(smoothed[0].points.len(), 2);
        assert_eq!(smoothed[0].points[1].pressure, 0.8);
    }

    #[test]
    fn pdf_ink_default_tolerance_comes_from_kernel() {
        assert!((pdf_ink_default_tolerance().unwrap() - 0.002).abs() < 1.0e-7);
    }

    #[test]
    fn pdf_annotation_storage_key_comes_from_kernel() {
        assert_eq!(
            pdf_annotation_storage_key("assets/paper.pdf").unwrap(),
            "d7af56fa7308eb53"
        );
    }

    #[test]
    fn pdf_lightweight_hash_comes_from_kernel() {
        assert_eq!(
            compute_pdf_lightweight_hash(b"%PDF", b"EOF", 1234).unwrap(),
            "d1a340980bc6729a17b938075e8d855ebb53f367c78d49b6bd1040254c4ba5ca"
        );
    }

    #[test]
    fn calculate_symmetry_from_text_uses_sealed_bridge_water_pipeline() {
        let xyz =
            "3\nwater\nO  0.000  0.000  0.117\nH  0.000  0.757 -0.469\nH  0.000 -0.757 -0.469\n";
        let result = calculate_symmetry_from_text(xyz, "xyz").expect("sealed bridge symmetry");

        assert_eq!(result.point_group, "C_2v");
        assert!(!result.axes.is_empty());
        assert!(!result.planes.is_empty());
    }

    #[test]
    fn calculate_symmetry_from_text_uses_sealed_bridge_linear_pipeline() {
        let xyz =
            "3\nCO2\nC  0.000  0.000  0.000\nO  0.000  0.000  1.160\nO  0.000  0.000 -1.160\n";
        let result = calculate_symmetry_from_text(xyz, "xyz").expect("sealed bridge symmetry");

        assert_eq!(result.point_group, "D∞h");
        assert!(result.has_inversion);
    }

    #[test]
    fn calculate_symmetry_from_text_maps_sealed_bridge_parse_errors() {
        let err = calculate_symmetry_from_text("1\n", "xyz").expect_err("invalid xyz");
        assert_eq!(err.to_string(), "XYZ 文件格式不完整");

        let err = calculate_symmetry_from_text("1,2\n", "mol2").expect_err("unsupported");
        assert_eq!(err.to_string(), "不支持的分子文件格式: mol2");
    }

    #[test]
    fn build_lattice_from_cif_uses_sealed_bridge_full_result() {
        let cif = r#"
data_NaCl
_cell_length_a 5.64
_cell_length_b 5.64
_cell_length_c 5.64
_cell_angle_alpha 90
_cell_angle_beta 90
_cell_angle_gamma 90

loop_
_symmetry_equiv_pos_as_xyz
x,y,z

loop_
_atom_site_type_symbol
_atom_site_fract_x
_atom_site_fract_y
_atom_site_fract_z
Na 0.0 0.0 0.0
Cl 0.5 0.5 0.5
"#;
        let data = build_lattice_from_cif(cif, 2, 2, 2).expect("sealed bridge lattice");
        assert_eq!(data.atoms.len(), 16);
        assert!((data.unit_cell.a - 5.64).abs() < 1e-8);

        let plane =
            calculate_miller_plane_from_cif(cif, 1, 1, 0).expect("sealed bridge Miller plane");
        assert!(plane.normal[2].abs() < 1e-6);
    }

    #[test]
    fn build_lattice_from_cif_maps_sealed_bridge_parse_errors() {
        let cif = r#"
data_test
loop_
_atom_site_type_symbol
_atom_site_fract_x
_atom_site_fract_y
_atom_site_fract_z
Na 0.0 0.0 0.0
"#;

        let err = build_lattice_from_cif(cif, 1, 1, 1).expect_err("missing cell");
        assert!(err.to_string().contains("晶胞参数"));

        let err = calculate_miller_plane_from_cif(cif, 1, 0, 0).expect_err("missing cell");
        assert!(err.to_string().contains("晶胞参数"));
    }

    #[test]
    fn calculate_miller_plane_from_cif_maps_sealed_bridge_miller_errors() {
        let cif = r#"
data_NaCl
_cell_length_a 5.64
_cell_length_b 5.64
_cell_length_c 5.64
_cell_angle_alpha 90
_cell_angle_beta 90
_cell_angle_gamma 90

loop_
_atom_site_type_symbol
_atom_site_fract_x
_atom_site_fract_y
_atom_site_fract_z
Na 0.0 0.0 0.0
"#;

        let err = calculate_miller_plane_from_cif(cif, 0, 0, 0).expect_err("zero index");
        assert_eq!(err.to_string(), "密勒指数 (h, k, l) 不能全为零");
    }
}
