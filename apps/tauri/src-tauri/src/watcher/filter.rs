//! 文件路径过滤逻辑：决定哪些文件变更应被 watcher 忽略。

use std::collections::HashSet;
use std::path::Path;

use crate::shared::command_utils::is_supported_extension;

/// 判断路径是否应该被忽略。
///
/// 规则（命中任一即忽略）：
/// 1. 目录本身的事件
/// 2. 不支持的文件扩展名
/// 3. 隐藏文件/文件夹（以 `.` 开头）
/// 4. 用户指定的忽略文件夹
pub fn should_ignore(path: &Path, vault: &Path, ignored: &HashSet<String>) -> bool {
    if path.is_dir() {
        return true;
    }

    let ext = path.extension().and_then(|e| e.to_str()).unwrap_or("");
    if !is_supported_extension(ext) {
        return true;
    }

    if let Ok(rel) = path.strip_prefix(vault) {
        for component in rel.components() {
            let name = component.as_os_str().to_string_lossy();
            if name.starts_with('.') {
                return true;
            }
            if ignored.contains(name.as_ref()) {
                return true;
            }
        }
    }

    false
}
