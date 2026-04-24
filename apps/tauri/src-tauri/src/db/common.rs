use rusqlite::Connection;
use std::path::Path;
use std::sync::{Arc, Mutex};
use std::time::Instant;

/// 从路径字符串中提取文件扩展名（小写），无扩展名返回空字符串。
pub(super) fn ext_from_path(path: &str) -> String {
    Path::new(path)
        .extension()
        .and_then(|e| e.to_str())
        .unwrap_or("")
        .to_lowercase()
}

/// 数据库状态包装器
/// 使用 Arc<Mutex<>> 实现跨线程共享：
/// - Mutex 保证对 SQLite 连接的互斥访问
/// - Arc 允许在 tokio 异步任务中安全引用同一个连接
///   （向量化后台任务需要在完成后写回数据库）
pub struct DbState {
    pub conn: Arc<Mutex<Connection>>,
}

/// 查询耗时计时器。在 Drop 时自动输出耗时日志。
/// 用法：`let _t = QueryTimer::new("search_notes");`
pub(crate) struct QueryTimer {
    label: &'static str,
    start: Instant,
}

impl QueryTimer {
    pub fn new(label: &'static str) -> Self {
        Self {
            label,
            start: Instant::now(),
        }
    }
}

impl Drop for QueryTimer {
    fn drop(&mut self) {
        let elapsed = self.start.elapsed();
        if elapsed.as_millis() > 0 {
            eprintln!("[查询耗时] {} = {}ms", self.label, elapsed.as_millis());
        }
    }
}
