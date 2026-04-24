use crate::db::{self, DbState, TruthStateDto};
use crate::AppError;
use tauri::State;

#[tauri::command]
pub fn study_session_start(
    note_id: String,
    folder: String,
    db: State<'_, DbState>,
) -> Result<i64, AppError> {
    let conn = db.conn.lock().map_err(|_| AppError::Lock)?;
    db::start_session(&conn, &note_id, &folder).map_err(Into::into)
}

#[tauri::command]
pub fn study_session_tick(
    session_id: i64,
    active_secs: i64,
    db: State<'_, DbState>,
) -> Result<(), AppError> {
    let conn = db.conn.lock().map_err(|_| AppError::Lock)?;
    db::tick_session(&conn, session_id, active_secs).map_err(Into::into)
}

#[tauri::command]
pub fn study_session_end(
    session_id: i64,
    active_secs: i64,
    db: State<'_, DbState>,
) -> Result<(), AppError> {
    let conn = db.conn.lock().map_err(|_| AppError::Lock)?;
    db::end_session(&conn, session_id, active_secs).map_err(Into::into)
}

#[tauri::command]
pub fn study_stats_query(
    days_back: i64,
    db: State<'_, DbState>,
) -> Result<db::StudyStats, AppError> {
    let conn = db.conn.lock().map_err(|_| AppError::Lock)?;
    db::query_stats(&conn, days_back).map_err(Into::into)
}

#[tauri::command]
pub fn truth_state_from_study(db: State<'_, DbState>) -> Result<TruthStateDto, AppError> {
    let conn = db.conn.lock().map_err(|_| AppError::Lock)?;
    db::query_truth_state(&conn).map_err(Into::into)
}

/// 返回预计算的热力图网格数据（26 周 x 7 天）
#[tauri::command]
pub fn get_heatmap_cells(db: State<'_, DbState>) -> Result<db::HeatmapGrid, AppError> {
    let conn = db.conn.lock().map_err(|_| AppError::Lock)?;
    db::query_heatmap_cells(&conn).map_err(Into::into)
}
