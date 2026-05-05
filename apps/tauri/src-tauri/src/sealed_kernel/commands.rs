// Tauri command wrappers for the sealed kernel bridge.
// The parent module owns bridge behavior; this file only exposes IPC commands.

use super::*;

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
    close_vault_state(state.inner())?;
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
