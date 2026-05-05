//! 文件系统增量监听模块
//!
//! 模块结构：
//! - `handler` — debouncer 事件回调（事件分类、kernel path filtering、IPC 发送）

mod handler;

use std::path::PathBuf;
use std::sync::{Arc, Mutex};
use std::time::Duration;

use notify_debouncer_mini::new_debouncer;
use serde::Serialize;
use tauri::AppHandle;

use crate::sealed_kernel;

/// 文件变更事件，发送给前端
#[derive(Debug, Clone, Serialize)]
pub struct FsChangeEvent {
    /// 变更的文件相对路径列表
    pub changed: Vec<String>,
    /// 被删除的文件相对路径列表
    pub removed: Vec<String>,
}

/// Watcher 状态，管理 notify 的生命周期。
///
/// 持有 debouncer 实例即表示监听活跃，drop 即停止。
/// 通过 `Arc<Mutex<>>` 实现跨线程共享。
pub struct WatcherState {
    inner: Arc<Mutex<Option<WatcherInner>>>,
}

struct WatcherInner {
    _debouncer: notify_debouncer_mini::Debouncer<notify::RecommendedWatcher>,
    vault_path: PathBuf,
}

impl WatcherState {
    pub fn new() -> Self {
        Self {
            inner: Arc::new(Mutex::new(None)),
        }
    }

    /// 启动文件监听。如果已有监听器则先停止旧的。
    pub fn start(
        &self,
        vault_path: &str,
        ignored_roots: &str,
        app: AppHandle,
    ) -> Result<(), String> {
        sealed_kernel::validate_vault_root_path(vault_path)
            .map_err(|err| format!("路径不存在或不是一个有效目录: {vault_path} ({err})"))?;
        let vault = PathBuf::from(vault_path);

        self.stop();

        let callback = handler::build_event_handler(ignored_roots.to_string(), app);

        let mut debouncer = new_debouncer(Duration::from_millis(500), callback)
            .map_err(|e| format!("创建 debouncer 失败: {}", e))?;

        debouncer
            .watcher()
            .watch(&vault, notify::RecursiveMode::Recursive)
            .map_err(|e| format!("启动监听失败: {}", e))?;

        eprintln!("[watcher] 开始监听: {}", vault_path);

        let mut guard = self.inner.lock().unwrap();
        *guard = Some(WatcherInner {
            _debouncer: debouncer,
            vault_path: vault,
        });

        Ok(())
    }

    /// 停止文件监听
    pub fn stop(&self) {
        let mut guard = self.inner.lock().unwrap();
        if let Some(inner) = guard.take() {
            eprintln!("[watcher] 停止监听: {}", inner.vault_path.display());
        }
    }
}
