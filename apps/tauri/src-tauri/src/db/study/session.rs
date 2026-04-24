use rusqlite::{params, Connection};

use crate::AppResult;

/// 开始一次学习会话，返回新插入的 session id
pub fn start_session(conn: &Connection, note_id: &str, folder: &str) -> AppResult<i64> {
    let now = super::unix_now_secs()?;

    conn.execute(
        "INSERT INTO study_sessions (note_id, folder, started_at, active_secs)
         VALUES (?1, ?2, ?3, 0)",
        params![note_id, folder, now],
    )?;

    Ok(conn.last_insert_rowid())
}

/// 心跳 / 结束会话：将 active_secs 增加 delta_secs
pub fn tick_session(conn: &Connection, session_id: i64, delta_secs: i64) -> AppResult<()> {
    conn.execute(
        "UPDATE study_sessions SET active_secs = active_secs + ?1 WHERE id = ?2",
        params![delta_secs, session_id],
    )?;
    Ok(())
}

/// 结束会话：与 tick 逻辑一致，语义区分
#[inline]
pub fn end_session(conn: &Connection, session_id: i64, delta_secs: i64) -> AppResult<()> {
    tick_session(conn, session_id, delta_secs)
}
