//! Debouncer 事件回调：将 notify 原始事件转换为 `FsChangeEvent` 并发送给前端。

use std::path::Path;

use notify_debouncer_mini::{DebouncedEvent, DebouncedEventKind};
use tauri::{AppHandle, Emitter, Manager};

use super::FsChangeEvent;
use crate::sealed_kernel;

/// 构建 debouncer 回调闭包。
///
/// 返回的闭包会：
/// 1. 过滤目录事件并通过 kernel 把路径转成 vault 相对路径
/// 2. 将事件分类为 changed / removed
/// 3. 通过 kernel 过滤隐藏路径、忽略目录、支持的 vault 文件类型并去重
/// 4. 通过 Tauri 事件总线发送 `vault:fs-change`
pub fn build_event_handler(
    ignored_roots: String,
    app: AppHandle,
) -> impl FnMut(Result<Vec<DebouncedEvent>, notify::Error>) {
    move |res| match res {
        Ok(events) => {
            let kernel_state = app.state::<sealed_kernel::SealedKernelState>();
            let (changed, removed) = classify_events(&events, &ignored_roots, |path| {
                sealed_kernel::relativize_vault_path(
                    &path.to_string_lossy(),
                    false,
                    kernel_state.inner(),
                )
                .ok()
            });
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

/// 将一批去抖后的事件分类为 (changed, removed)，同时交给 kernel 过滤和去重。
fn classify_events<F>(
    events: &[DebouncedEvent],
    ignored_roots: &str,
    mut relativize_path: F,
) -> (Vec<String>, Vec<String>)
where
    F: FnMut(&Path) -> Option<String>,
{
    let mut changed = Vec::new();
    let mut removed = Vec::new();

    for event in events {
        let path: &Path = event.path.as_path();

        if path.is_dir() {
            continue;
        }

        let Some(rel) = relativize_path(path) else {
            continue;
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

    changed = filter_supported_vault_paths(changed, ignored_roots);
    removed = filter_supported_vault_paths(removed, ignored_roots);

    (changed, removed)
}

fn filter_supported_vault_paths(paths: Vec<String>, ignored_roots: &str) -> Vec<String> {
    if paths.is_empty() {
        return paths;
    }

    match sealed_kernel::filter_supported_vault_paths_filtered(&paths, ignored_roots) {
        Ok(filtered) => filtered,
        Err(err) => {
            eprintln!("[watcher] kernel filtered path filter failed: {}", err);
            Vec::new()
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::path::PathBuf;

    fn make_temp_file(prefix: &str) -> PathBuf {
        let nanos = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap_or_default()
            .as_nanos();
        let dir = std::env::temp_dir().join(format!(
            "nexus-watcher-{prefix}-{}-{nanos}",
            std::process::id()
        ));
        std::fs::create_dir_all(&dir).expect("create temp dir");
        let path = dir.join("event.md");
        std::fs::write(&path, b"# changed").expect("write temp file");
        path
    }

    #[test]
    fn classify_events_uses_kernel_relativizer_result_before_filtering() {
        let changed_path = make_temp_file("changed");
        let removed_path = changed_path
            .parent()
            .expect("temp file parent")
            .join("removed.md");
        let events = vec![
            DebouncedEvent::new(changed_path.clone(), DebouncedEventKind::Any),
            DebouncedEvent::new(removed_path.clone(), DebouncedEventKind::Any),
        ];

        let (changed, removed) = classify_events(&events, "", |path| {
            if path == changed_path.as_path() {
                Some("Folder\\Changed.md".to_string())
            } else if path == removed_path.as_path() {
                Some("Folder\\Removed.md".to_string())
            } else {
                None
            }
        });

        assert_eq!(changed, vec!["Folder/Changed.md".to_string()]);
        assert_eq!(removed, vec!["Folder/Removed.md".to_string()]);

        let _ = std::fs::remove_dir_all(changed_path.parent().expect("temp parent"));
    }
}
