use rusqlite::{params, Connection};

use crate::models::NoteInfo;
use crate::AppResult;

use super::common::{ext_from_path, QueryTimer};

/// Upsert note metadata into the legacy embedding cache table.
pub fn upsert_embedding_note_metadata(
    conn: &Connection,
    id: &str,
    filename: &str,
    absolute_path: &str,
    created_at: i64,
    updated_at: i64,
) -> AppResult<()> {
    conn.execute(
        "INSERT OR REPLACE INTO notes_index (id, filename, absolute_path, created_at, updated_at, content)
         VALUES (?1, ?2, ?3, ?4, ?5, '')",
        params![id, filename, absolute_path, created_at, updated_at],
    )?;

    Ok(())
}

/// Batch-read note timestamps from the legacy embedding cache table.
pub fn get_embedding_note_timestamps(
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

/// 将向量化结果（Vec<f32>）写入指定笔记的 embedding 字段。
///
/// # Vec<f32> -> BLOB 序列化方案
/// 直接将 f32 数组的内存表示（每个 f32 占 4 字节，小端序）
/// 拷贝为连续的字节切片存入 SQLite BLOB。
/// 这种方式零拷贝、零开销，比 JSON 序列化快几个数量级。
///
/// 反序列化时只需将 BLOB 按 4 字节对齐重新解释为 &[f32] 即可。
pub fn update_note_embedding(conn: &Connection, id: &str, embedding: &[f32]) -> AppResult<()> {
    // 安全地将 &[f32] 转换为 &[u8]：
    // 每个 f32 = 4 bytes，总长度 = embedding.len() * 4
    let bytes: &[u8] = unsafe {
        std::slice::from_raw_parts(
            embedding.as_ptr() as *const u8,
            embedding.len() * std::mem::size_of::<f32>(),
        )
    };

    conn.execute(
        "UPDATE notes_index SET embedding = ?1 WHERE id = ?2",
        params![bytes, id],
    )?;

    Ok(())
}

/// Fetch ALL notes that have embeddings for full-corpus vector retrieval.
/// Previous approach (ORDER BY updated_at DESC LIMIT N) missed old but
/// highly relevant notes. Brute-force cosine scan over all embeddings is
/// fast enough for typical vault sizes (< 10k notes).
pub fn get_all_embeddings(conn: &Connection) -> AppResult<Vec<(NoteInfo, Vec<f32>)>> {
    let _t = QueryTimer::new("get_all_embeddings");
    let mut stmt = conn.prepare(
        "SELECT id, filename, absolute_path, created_at, updated_at, embedding
             FROM notes_index
             WHERE embedding IS NOT NULL",
    )?;

    let rows = stmt.query_map([], |row| {
        let blob: Vec<u8> = row.get(5)?;
        let abs_path: String = row.get(2)?;
        Ok((
            NoteInfo {
                id: row.get(0)?,
                name: row.get(1)?,
                file_extension: ext_from_path(&abs_path),
                path: abs_path,
                created_at: row.get(3)?,
                updated_at: row.get(4)?,
            },
            blob,
        ))
    })?;

    let mut results = Vec::new();
    for row in rows {
        let (note, bytes) = row?;
        if bytes.len() % 4 != 0 {
            continue;
        }
        let embedding: Vec<f32> = bytes
            .chunks_exact(4)
            .map(|c| f32::from_le_bytes([c[0], c[1], c[2], c[3]]))
            .collect();
        results.push((note, embedding));
    }

    Ok(results)
}

/// 清空所有向量缓存，供"重建向量索引"前使用。
pub fn clear_all_embeddings(conn: &Connection) -> AppResult<()> {
    conn.execute("UPDATE notes_index SET embedding = NULL", [])?;
    Ok(())
}
