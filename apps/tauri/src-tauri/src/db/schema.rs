use rusqlite::Connection;
use std::path::Path;

use crate::AppResult;

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

    // Kernel owns note content reads now; keep the legacy cache column empty.
    conn.execute(
        "UPDATE notes_index SET content = '' WHERE content <> ''",
        [],
    )?;

    // 为 embedding 列建立部分索引，加速语义搜索中的 "embedding IS NOT NULL" 过滤
    conn.execute(
        "CREATE INDEX IF NOT EXISTS idx_notes_has_embedding ON notes_index (id) WHERE embedding IS NOT NULL",
        [],
    )?;

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
