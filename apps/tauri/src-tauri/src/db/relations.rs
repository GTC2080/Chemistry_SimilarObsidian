use rusqlite::{params, Connection};

use crate::AppResult;

use super::parsing::{extract_all_tags, extract_links};

/// 同步某篇笔记的链接关系到 note_links 表。
///
/// # 更新策略（先删后插）
/// 1. DELETE 该 source_id 的所有旧链接记录
/// 2. 从 content 中提取新的 [[...]] 目标列表
/// 3. 批量 INSERT 新的链接关系
///
/// 这种"全量替换"策略简单可靠，对于单篇笔记的链接数量（通常 < 100）
/// 性能完全不是问题。
pub(crate) fn sync_links(conn: &Connection, source_id: &str, content: &str) -> AppResult<()> {
    // 第一步：清除该笔记的所有旧链接
    conn.execute(
        "DELETE FROM note_links WHERE source_id = ?1",
        params![source_id],
    )?;

    // 第二步：提取新链接并批量插入
    let targets = extract_links(content);
    if targets.is_empty() {
        return Ok(());
    }

    let mut stmt =
        conn.prepare("INSERT OR IGNORE INTO note_links (source_id, target_name) VALUES (?1, ?2)")?;

    for target in &targets {
        stmt.execute(params![source_id, target])?;
    }

    Ok(())
}

/// 同步某篇笔记的标签关系到 note_tags 表。
/// 策略与 sync_links 相同：先 DELETE 后 INSERT。
pub(crate) fn sync_tags(conn: &Connection, note_id: &str, content: &str) -> AppResult<()> {
    conn.execute("DELETE FROM note_tags WHERE note_id = ?1", params![note_id])?;

    let tags = extract_all_tags(content);
    if tags.is_empty() {
        return Ok(());
    }

    let mut stmt =
        conn.prepare("INSERT OR IGNORE INTO note_tags (note_id, tag_name) VALUES (?1, ?2)")?;

    for tag in &tags {
        stmt.execute(params![note_id, tag])?;
    }

    Ok(())
}
