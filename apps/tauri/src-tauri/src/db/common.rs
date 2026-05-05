use rusqlite::Connection;
use std::sync::{Arc, Mutex};

/// 数据库状态包装器
/// 使用 Arc<Mutex<>> 实现跨线程共享：
/// - Mutex 保证对 SQLite study 连接的互斥访问
/// - Arc 保留 Tauri State 的既有共享形态
pub struct DbState {
    pub conn: Arc<Mutex<Connection>>,
}
