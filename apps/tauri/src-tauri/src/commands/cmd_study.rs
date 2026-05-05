use serde_json::Value;
use tauri::State;

use crate::sealed_kernel::{self, SealedKernelState};
use crate::AppError;

#[tauri::command]
pub fn study_session_start(
    note_id: String,
    folder: String,
    sealed_kernel: State<'_, SealedKernelState>,
) -> Result<i64, AppError> {
    sealed_kernel::start_study_session(&note_id, &folder, sealed_kernel.inner())
}

#[tauri::command]
pub fn study_session_tick(
    session_id: i64,
    active_secs: i64,
    sealed_kernel: State<'_, SealedKernelState>,
) -> Result<(), AppError> {
    sealed_kernel::tick_study_session(session_id, active_secs, sealed_kernel.inner())
}

#[tauri::command]
pub fn study_session_end(
    session_id: i64,
    active_secs: i64,
    sealed_kernel: State<'_, SealedKernelState>,
) -> Result<(), AppError> {
    sealed_kernel::end_study_session(session_id, active_secs, sealed_kernel.inner())
}

#[tauri::command]
pub fn study_stats_query(
    days_back: i64,
    sealed_kernel: State<'_, SealedKernelState>,
) -> Result<Value, AppError> {
    sealed_kernel::query_study_stats(days_back, sealed_kernel.inner())
}

#[tauri::command]
pub fn truth_state_from_study(
    sealed_kernel: State<'_, SealedKernelState>,
) -> Result<Value, AppError> {
    sealed_kernel::query_study_truth_state(sealed_kernel.inner())
}

#[tauri::command]
pub fn get_heatmap_cells(sealed_kernel: State<'_, SealedKernelState>) -> Result<Value, AppError> {
    sealed_kernel::query_study_heatmap_grid(sealed_kernel.inner())
}
