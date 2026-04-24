use rusqlite::{params, types::ToSql, Connection, OptionalExtension};

use crate::AppResult;

use super::relations::{sync_links, sync_tags};
use super::schema::sync_fts_row;

/// Upsert 单条笔记记录到数据库，并同步其链接关系。
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

    // 同步链接关系：解析 content 中的 [[...]] 并写入 note_links 表
    sync_links(conn, id, content)?;

    // 同步标签关系：解析 Frontmatter 和行内 #标签 并写入 note_tags 表
    sync_tags(conn, id, content)?;
    sync_fts_row(conn, id)?;

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

/// 批量获取笔记的内容和文件名，用于 RAG 上下文组装。
/// 返回 Vec<(filename, content)>。
pub fn get_notes_content_by_ids(
    conn: &Connection,
    ids: &[String],
) -> AppResult<Vec<(String, String)>> {
    if ids.is_empty() {
        return Ok(Vec::new());
    }

    // 动态构建 IN 子句的占位符
    let placeholders: Vec<String> = ids
        .iter()
        .enumerate()
        .map(|(i, _)| format!("?{}", i + 1))
        .collect();
    let sql = format!(
        "SELECT filename, content FROM notes_index WHERE id IN ({})",
        placeholders.join(", ")
    );

    let mut stmt = conn.prepare(&sql)?;

    let params: Vec<&dyn ToSql> = ids.iter().map(|id| id as &dyn ToSql).collect();

    let rows = stmt.query_map(params.as_slice(), |row| {
        Ok((row.get::<_, String>(0)?, row.get::<_, String>(1)?))
    })?;

    let mut results = Vec::new();
    for row in rows {
        results.push(row?);
    }
    Ok(results)
}
