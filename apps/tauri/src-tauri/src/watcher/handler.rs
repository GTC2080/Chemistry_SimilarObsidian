//! Debouncer 事件回调：将 notify 原始事件转换为 `FsChangeEvent` 并发送给前端。

use std::collections::HashSet;
use std::path::{Path, PathBuf};

use notify_debouncer_mini::{DebouncedEvent, DebouncedEventKind};
use tauri::{AppHandle, Emitter};

use super::filter::should_ignore;
use super::FsChangeEvent;

/// 构建 debouncer 回调闭包。
///
/// 返回的闭包会：
/// 1. 过滤不关心的路径（目录、隐藏文件、非支持扩展名）
/// 2. 将事件分类为 changed / removed
/// 3. 去重后通过 Tauri 事件总线发送 `vault:fs-change`
pub fn build_event_handler(
    vault: PathBuf,
    ignored: HashSet<String>,
    app: AppHandle,
) -> impl FnMut(Result<Vec<DebouncedEvent>, notify::Error>) {
    move |res| match res {
        Ok(events) => {
            let (changed, removed) = classify_events(&events, &vault, &ignored);
            if !changed.is_empty() || !removed.is_empty() {
                let payload = FsChangeEvent { changed, removed };
                if let Err(e) = app.emit("vault:fs-change", &payload) {
                    eprintln!("[watcher] 发送事件失败: {}", e);
                }
            }
        }
        Err(e) => {
            eprintln!("[watcher] 监听错误: {:?}", e);
        }
    }
}

/// 将一批去抖后的事件分类为 (changed, removed)，同时过滤和去重。
fn classify_events(
    events: &[DebouncedEvent],
    vault: &Path,
    ignored: &HashSet<String>,
) -> (Vec<String>, Vec<String>) {
    let mut changed = Vec::new();
    let mut removed = Vec::new();

    for event in events {
        let path: &Path = event.path.as_path();

        if should_ignore(path, vault, ignored) {
            continue;
        }

        let rel = match path.strip_prefix(vault) {
            Ok(r) => r.to_string_lossy().into_owned(),
            Err(_) => continue,
        };

        match event.kind {
            DebouncedEventKind::Any => {
                if path.exists() {
                    changed.push(rel);
                } else {
                    removed.push(rel);
                }
            }
            DebouncedEventKind::AnyContinuous => {
                if path.exists() {
                    changed.push(rel);
                }
            }
            _ => {}
        }
    }

    changed.sort();
    changed.dedup();
    removed.sort();
    removed.dedup();

    (changed, removed)
}
