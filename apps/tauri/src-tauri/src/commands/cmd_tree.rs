use std::collections::HashSet;

use tauri::State;

use crate::models::FileTreeNode;
use crate::sealed_kernel::{self, SealedKernelState};
use crate::shared::command_utils::parse_ignored_folders;
use crate::AppError;

fn retain_allowed_roots(nodes: &mut Vec<FileTreeNode>, ignored: &HashSet<String>) {
    nodes.retain(|node| {
        let first = node.relative_path.split('/').next().unwrap_or("");
        !ignored.contains(first)
    });
}

#[tauri::command]
pub fn build_file_tree(
    vault_path: String,
    ignored_folders: Option<String>,
    sealed_kernel: State<SealedKernelState>,
) -> Result<Vec<FileTreeNode>, AppError> {
    if vault_path.trim().is_empty() {
        return Ok(Vec::new());
    }

    let ignored = parse_ignored_folders(ignored_folders);
    let mut tree = sealed_kernel::query_file_tree(&vault_path, sealed_kernel.inner(), 4096)?;
    retain_allowed_roots(&mut tree, &ignored);
    Ok(tree)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn tree_node(name: &str, relative_path: &str, is_folder: bool) -> FileTreeNode {
        FileTreeNode {
            name: name.to_string(),
            full_name: name.to_string(),
            relative_path: relative_path.to_string(),
            is_folder,
            note: None,
            children: Vec::new(),
            file_count: if is_folder { 0 } else { 1 },
        }
    }

    #[test]
    fn filters_ignored_kernel_tree_roots() {
        let mut tree = vec![
            tree_node("node_modules", "node_modules", true),
            tree_node("lab", "lab", true),
            tree_node("node_modules.md", "node_modules.md", false),
        ];
        let ignored = HashSet::from(["node_modules".to_string()]);

        retain_allowed_roots(&mut tree, &ignored);

        assert_eq!(tree.len(), 2);
        assert_eq!(tree[0].relative_path, "lab");
        assert_eq!(tree[1].relative_path, "node_modules.md");
    }
}
