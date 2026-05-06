use std::os::raw::{c_char, c_int};

use crate::{AppError, AppResult};

use super::errors::{atom_limit_bridge_error, bridge_error, molecular_preview_bridge_error};
use super::ffi::*;

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

fn kernel_default_f32(
    operation: &str,
    getter: unsafe extern "C" fn(*mut f32, *mut *mut c_char) -> c_int,
) -> AppResult<f32> {
    let mut value = 0.0f32;
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe { getter(&mut value, &mut error) };
    if code != 0 {
        return Err(bridge_error(operation, code, error));
    }
    Ok(value)
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

pub fn file_tree_default_limit() -> AppResult<u64> {
    kernel_default_limit(
        "sealed_kernel_get_file_tree_default_limit",
        sealed_kernel_bridge_get_file_tree_default_limit,
    )
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

pub fn semantic_context_min_bytes() -> AppResult<usize> {
    kernel_default_usize_limit(
        "sealed_kernel_get_semantic_context_min_bytes",
        sealed_kernel_bridge_get_semantic_context_min_bytes,
    )
}

pub fn ai_chat_timeout_secs() -> AppResult<u64> {
    kernel_default_limit(
        "sealed_kernel_get_ai_chat_timeout_secs",
        sealed_kernel_bridge_get_ai_chat_timeout_secs,
    )
}

pub fn ai_ponder_timeout_secs() -> AppResult<u64> {
    kernel_default_limit(
        "sealed_kernel_get_ai_ponder_timeout_secs",
        sealed_kernel_bridge_get_ai_ponder_timeout_secs,
    )
}

pub fn ai_embedding_request_timeout_secs() -> AppResult<u64> {
    kernel_default_limit(
        "sealed_kernel_get_ai_embedding_request_timeout_secs",
        sealed_kernel_bridge_get_ai_embedding_request_timeout_secs,
    )
}

pub fn ai_embedding_cache_limit() -> AppResult<usize> {
    kernel_default_usize_limit(
        "sealed_kernel_get_ai_embedding_cache_limit",
        sealed_kernel_bridge_get_ai_embedding_cache_limit,
    )
}

pub fn ai_embedding_concurrency_limit() -> AppResult<usize> {
    kernel_default_usize_limit(
        "sealed_kernel_get_ai_embedding_concurrency_limit",
        sealed_kernel_bridge_get_ai_embedding_concurrency_limit,
    )
}

pub fn ai_rag_top_note_limit() -> AppResult<usize> {
    kernel_default_usize_limit(
        "sealed_kernel_get_ai_rag_top_note_limit",
        sealed_kernel_bridge_get_ai_rag_top_note_limit,
    )
}

pub fn ai_ponder_temperature() -> AppResult<f32> {
    kernel_default_f32(
        "sealed_kernel_get_ai_ponder_temperature",
        sealed_kernel_bridge_get_ai_ponder_temperature,
    )
}

pub fn pdf_ink_default_tolerance() -> AppResult<f32> {
    let tolerance = kernel_default_f32(
        "sealed_kernel_get_pdf_ink_default_tolerance",
        sealed_kernel_bridge_get_pdf_ink_default_tolerance,
    )?;
    if !tolerance.is_finite() || tolerance <= 0.0 {
        return Err(AppError::Custom(
            "kernel PDF ink default tolerance must be positive.".to_string(),
        ));
    }
    Ok(tolerance)
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

pub(super) fn symmetry_atom_limit() -> AppResult<usize> {
    let mut limit = 0u64;
    let mut error: *mut c_char = std::ptr::null_mut();
    let code = unsafe { sealed_kernel_bridge_get_symmetry_atom_limit(&mut limit, &mut error) };
    if code != 0 {
        return Err(atom_limit_bridge_error("symmetry", code, error));
    }
    usize::try_from(limit)
        .map_err(|_| AppError::Custom("kernel symmetry atom limit exceeds host usize".to_string()))
}

pub(super) fn crystal_supercell_atom_limit() -> AppResult<usize> {
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
