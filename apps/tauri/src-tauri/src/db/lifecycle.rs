use rusqlite::{params, Connection};

use crate::AppResult;

/// Delete a note from the legacy embedding cache table.
pub fn delete_note_by_id(conn: &Connection, id: &str) -> AppResult<()> {
    conn.execute("DELETE FROM notes_index WHERE id = ?1", params![id])?;
    Ok(())
}
