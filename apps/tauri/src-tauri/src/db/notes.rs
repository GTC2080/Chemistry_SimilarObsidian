use rusqlite::{params, Connection, OptionalExtension};

use crate::AppResult;

/// Upsert note metadata/content into the legacy embedding cache table.
pub fn upsert_note(
    conn: &Connection,
    id: &str,
    filename: &str,
    absolute_path: &str,
    created_at: i64,
    updated_at: i64,
    content: &str,
) -> AppResult<()> {
    conn.execute(
        "INSERT OR REPLACE INTO notes_index (id, filename, absolute_path, created_at, updated_at, content)
         VALUES (?1, ?2, ?3, ?4, ?5, ?6)",
        params![id, filename, absolute_path, created_at, updated_at, content],
    )?;

    Ok(())
}

#[allow(dead_code)]
pub fn get_note_updated_at(conn: &Connection, id: &str) -> AppResult<Option<i64>> {
    conn.query_row(
        "SELECT updated_at FROM notes_index WHERE id = ?1",
        params![id],
        |row| row.get(0),
    )
    .optional()
    .map_err(Into::into)
}

/// Batch-read all note updated_at timestamps into a HashMap.
pub fn get_all_note_timestamps(
    conn: &Connection,
) -> AppResult<std::collections::HashMap<String, i64>> {
    let mut stmt = conn.prepare("SELECT id, updated_at FROM notes_index")?;
    let rows = stmt.query_map([], |row| {
        Ok((row.get::<_, String>(0)?, row.get::<_, i64>(1)?))
    })?;
    let mut map = std::collections::HashMap::new();
    for row in rows {
        let (id, ts) = row?;
        map.insert(id, ts);
    }
    Ok(map)
}
