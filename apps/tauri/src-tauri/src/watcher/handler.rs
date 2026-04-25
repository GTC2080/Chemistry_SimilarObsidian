//! Debouncer 事件回调：将 notify 原始事件转换为 `FsChangeEvent` 并发送给前端。

use std::collections::HashSet;
use std::path::{Path, PathBuf};

use notify_debouncer_mini::{DebouncedEvent, DebouncedEventKind};
use tauri::{AppHandle, Emitter};

use super::filter::should_ignore;
use super::FsChangeEvent;
use crate::sealed_kernel;

/// 构建 debouncer 回调闭包。
///
/// 返回的闭包会：
/// 1. 过滤不关心的路径（目录、隐藏文件、用户忽略目录）
/// 2. 将事件分类为 changed / removed
/// 3. 通过 kernel 过滤支持的 vault 文件类型并去重
/// 4. 通过 Tauri 事件总线发送 `vault:fs-change`
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

    changed = filter_supported_vault_paths(changed);
    removed = filter_supported_vault_paths(removed);

    (changed, removed)
}

fn filter_supported_vault_paths(paths: Vec<String>) -> Vec<String> {
    if paths.is_empty() {
        return paths;
    }

    match sealed_kernel::filter_supported_vault_paths(&paths) {
        Ok(filtered) => filtered,
        Err(err) => {
            eprintln!("[watcher] kernel path filter failed: {}", err);
            Vec::new()
        }
    }
}
