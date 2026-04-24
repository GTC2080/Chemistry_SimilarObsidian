use rusqlite::{params, Connection};
use std::path::Path;

use crate::AppResult;

pub(crate) fn fts_available(conn: &Connection) -> bool {
    conn.query_row(
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name='notes_fts' LIMIT 1",
        [],
        |_| Ok(()),
    )
    .is_ok()
}

/// 配置 SQLite 性能参数。
/// 所有参数均可安全回退：去掉此函数后数据库仍可正常打开，
/// 因为这些 PRAGMA 不改变持久化格式（WAL 除外，已有）。
fn apply_performance_pragmas(conn: &Connection) {
    // WAL 模式（已有，这里确认）
    let _ = conn.execute_batch("PRAGMA journal_mode=WAL;");

    // synchronous=NORMAL：WAL 模式下安全且显著减少 fsync 次数。
    // 最坏情况：断电可能丢失最近一次 checkpoint 后的写入，
    // 但数据库不会损坏。对笔记应用可接受。
    let _ = conn.execute_batch("PRAGMA synchronous=NORMAL;");

    // cache_size：负值单位为 KiB，-64000 ≈ 64 MB 内存缓存。
    // 默认仅 2 MB，大库查询频繁 miss page cache。
    let _ = conn.execute_batch("PRAGMA cache_size=-64000;");

    // temp_store=MEMORY：临时表和排序用内存而非磁盘，
    // 加速 ORDER BY / GROUP BY / DISTINCT 等操作。
    let _ = conn.execute_batch("PRAGMA temp_store=MEMORY;");

    // mmap_size：128 MB 内存映射 I/O，加速大量随机读取。
    // 超出文件大小的部分会被忽略，不会浪费内存。
    let _ = conn.execute_batch("PRAGMA mmap_size=134217728;");

    // busy_timeout：遇到锁竞争时最多等 5 秒再报错，
    // 避免高并发时频繁 SQLITE_BUSY。
    let _ = conn.execute_batch("PRAGMA busy_timeout=5000;");
}

pub(crate) fn sync_fts_row(conn: &Connection, id: &str) -> AppResult<()> {
    if !fts_available(conn) {
        return Ok(());
    }

    conn.execute("DELETE FROM notes_fts WHERE id = ?1", params![id])?;

    conn.execute(
        "INSERT INTO notes_fts (id, filename, content)
         SELECT id, filename, content FROM notes_index WHERE id = ?1",
        params![id],
    )?;

    Ok(())
}

pub(crate) fn delete_fts_row(conn: &Connection, id: &str) -> AppResult<()> {
    if !fts_available(conn) {
        return Ok(());
    }
    conn.execute("DELETE FROM notes_fts WHERE id = ?1", params![id])?;
    Ok(())
}

/// 初始化数据库：在指定的 Vault 目录下创建/打开 index.db 文件，
/// 并执行建表语句（IF NOT EXISTS 保证幂等性）。
pub fn init_db(vault_path: &str) -> AppResult<Connection> {
    let db_path = Path::new(vault_path).join("index.db");

    let conn = Connection::open(&db_path)?;

    // 应用性能参数（WAL + synchronous + cache_size + temp_store + mmap + busy_timeout）
    apply_performance_pragmas(&conn);

    // 笔记索引表
    conn.execute(
        "CREATE TABLE IF NOT EXISTS notes_index (
            id            TEXT PRIMARY KEY,
            filename      TEXT NOT NULL,
            absolute_path TEXT NOT NULL,
            created_at    INTEGER NOT NULL,
            updated_at    INTEGER NOT NULL,
            content       TEXT NOT NULL DEFAULT '',
            embedding     BLOB
        )",
        [],
    )?;

    // 兼容旧数据库：如果表已存在但缺少 embedding 列，动态添加
    // ALTER TABLE ... ADD COLUMN 在列已存在时会报错，这里静默忽略即可
    let _ = conn.execute("ALTER TABLE notes_index ADD COLUMN embedding BLOB", []);

    // 链接关系表：记录笔记之间的 [[双向链接]] 关系
    // source_id: 发起链接的笔记 ID（相对路径）
    // target_name: 被链接笔记的名称（[[...]] 内部的文字）
    // 复合主键 (source_id, target_name) 保证同一对关系不会重复
    conn.execute(
        "CREATE TABLE IF NOT EXISTS note_links (
            source_id   TEXT NOT NULL,
            target_name TEXT NOT NULL,
            PRIMARY KEY (source_id, target_name)
        )",
        [],
    )?;

    // 为 target_name 建立索引，加速反向链接查询
    // （查询"谁链接了我"时按 target_name 检索）
    conn.execute(
        "CREATE INDEX IF NOT EXISTS idx_links_target ON note_links (target_name)",
        [],
    )?;

    // 复合索引：覆盖反向链接 JOIN 查询 (WHERE target_name = ? → source_id)
    conn.execute(
        "CREATE INDEX IF NOT EXISTS idx_links_target_source ON note_links (target_name, source_id)",
        [],
    )?;

    // 标签关联表：记录笔记与标签的多对多关系
    // note_id: 笔记 ID（相对路径）
    // tag_name: 标签名称（支持 / 分隔的层级标签，如 "学习/化学"）
    // 复合主键 (note_id, tag_name) 保证同一对关系不会重复
    conn.execute(
        "CREATE TABLE IF NOT EXISTS note_tags (
            note_id  TEXT NOT NULL,
            tag_name TEXT NOT NULL,
            PRIMARY KEY (note_id, tag_name)
        )",
        [],
    )?;

    // 为 tag_name 建立索引，加速按标签查询笔记
    conn.execute(
        "CREATE INDEX IF NOT EXISTS idx_tags_name ON note_tags (tag_name)",
        [],
    )?;

    // 为 embedding 列建立部分索引，加速语义搜索中的 "embedding IS NOT NULL" 过滤
    conn.execute(
        "CREATE INDEX IF NOT EXISTS idx_notes_has_embedding ON notes_index (id) WHERE embedding IS NOT NULL",
        [],
    )?;

    // FTS5 全文索引（文件名 + 正文内容），用于快速搜索
    // 如果当前 SQLite 构建不支持 FTS5，这里会失败，后续自动回退到 LIKE 查询。
    let fts_created = conn
        .execute(
            "CREATE VIRTUAL TABLE IF NOT EXISTS notes_fts USING fts5(
                id UNINDEXED,
                filename,
                content
            )",
            [],
        )
        .is_ok();
    // 仅在 FTS 表刚创建且为空时填充（避免每次打开 vault 都重复插入）
    if fts_created {
        let fts_empty = conn
            .query_row("SELECT COUNT(*) FROM notes_fts", [], |r| r.get::<_, i64>(0))
            .unwrap_or(0)
            == 0;
        if fts_empty {
            let has_notes = conn
                .query_row("SELECT COUNT(*) FROM notes_index", [], |r| {
                    r.get::<_, i64>(0)
                })
                .unwrap_or(0)
                > 0;
            if has_notes {
                let _ = conn.execute(
                    "INSERT INTO notes_fts (id, filename, content)
                     SELECT id, filename, content FROM notes_index",
                    [],
                );
            }
        }
    }

    // 学习会话表：记录每次打开笔记的主动学习时长
    conn.execute_batch(
        "CREATE TABLE IF NOT EXISTS study_sessions (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            note_id     TEXT NOT NULL,
            folder      TEXT NOT NULL,
            started_at  INTEGER NOT NULL,
            active_secs INTEGER NOT NULL DEFAULT 0
        );
        CREATE INDEX IF NOT EXISTS idx_ss_started ON study_sessions(started_at);
        CREATE INDEX IF NOT EXISTS idx_ss_note ON study_sessions(note_id);",
    )?;

    // 更新查询优化器统计信息，帮助 SQLite 选择更优的执行计划。
    // 仅在索引/表结构变化后有意义，每次 init 执行一次开销很小。
    let _ = conn.execute_batch("ANALYZE;");

    Ok(conn)
}
