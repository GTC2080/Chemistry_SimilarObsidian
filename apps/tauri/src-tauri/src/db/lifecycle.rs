use rusqlite::{params, Connection};

use crate::AppResult;

use super::schema::delete_fts_row;

/// 删除单篇笔记及其关联索引数据（链接/标签）。
pub fn delete_note_by_id(conn: &Connection, id: &str) -> AppResult<()> {
    delete_fts_row(conn, id)?;
    conn.execute("DELETE FROM note_links WHERE source_id = ?1", params![id])?;
    conn.execute("DELETE FROM note_tags WHERE note_id = ?1", params![id])?;
    conn.execute("DELETE FROM notes_index WHERE id = ?1", params![id])?;
    Ok(())
}
