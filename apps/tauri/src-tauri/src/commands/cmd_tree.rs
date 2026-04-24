use tauri::State;

use crate::models::{FileTreeNode, NoteInfo};
use crate::sealed_kernel::{self, SealedKernelState};
use crate::shared::command_utils::parse_ignored_folders;
use crate::AppError;

fn sort_and_count_tree(nodes: &mut Vec<FileTreeNode>) -> u32 {
    nodes.sort_by(|a, b| {
        if a.is_folder != b.is_folder {
            return if a.is_folder {
                std::cmp::Ordering::Less
            } else {
                std::cmp::Ordering::Greater
            };
        }
        a.name.cmp(&b.name)
    });

    let mut total = 0u32;
    for node in nodes.iter_mut() {
        if node.is_folder {
            let count = sort_and_count_tree(&mut node.children);
            node.file_count = count;
            total += count;
        } else {
            node.file_count = 1;
            total += 1;
        }
    }
    total
}

fn build_file_tree_from_notes(notes: Vec<NoteInfo>) -> Vec<FileTreeNode> {
    let mut root: Vec<FileTreeNode> = Vec::new();

    for note in notes {
        let parts: Vec<String> = note
            .id
            .replace('\\', "/")
            .split('/')
            .map(|s| s.to_string())
            .collect();
        let mut current_level = &mut root;

        for i in 0..parts.len() {
            let segment = parts[i].clone();
            let is_last = i == parts.len() - 1;

            if is_last {
                current_level.push(FileTreeNode {
                    name: note.name.clone(),
                    full_name: segment.clone(),
                    relative_path: parts[..=i].join("/"),
                    is_folder: false,
                    note: Some(note.clone()),
                    children: Vec::new(),
                    file_count: 1,
                });
            } else {
                let rel_path = parts[..=i].join("/");
                let existing_index = current_level
                    .iter()
                    .position(|n| n.is_folder && n.name == segment);

                let folder_index = if let Some(idx) = existing_index {
                    idx
                } else {
                    current_level.push(FileTreeNode {
                        name: segment.clone(),
                        full_name: segment.clone(),
                        relative_path: rel_path,
                        is_folder: true,
                        note: None,
                        children: Vec::new(),
                        file_count: 0,
                    });
                    current_level.len() - 1
                };
                current_level = &mut current_level[folder_index].children;
            }
        }
    }

    sort_and_count_tree(&mut root);
    root
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
    let mut notes = sealed_kernel::query_note_infos(&vault_path, sealed_kernel.inner(), 4096)?;
    notes.retain(|note| {
        let first = note.id.split('/').next().unwrap_or("");
        !ignored.contains(first)
    });
    Ok(build_file_tree_from_notes(notes))
}

#[cfg(test)]
mod tests {
    use super::*;

    fn note(id: &str) -> NoteInfo {
        let normalized = id.replace('\\', "/");
        let stem = normalized
            .rsplit('/')
            .next()
            .and_then(|name| name.rsplit_once('.').map(|(stem, _)| stem).or(Some(name)))
            .unwrap_or("untitled");
        let ext = normalized
            .rsplit_once('.')
            .map(|(_, ext)| ext.to_string())
            .unwrap_or_default();

        NoteInfo {
            id: normalized.clone(),
            name: stem.to_string(),
            path: format!("C:/vault/{normalized}"),
            created_at: 1,
            updated_at: 2,
            file_extension: ext,
        }
    }

    #[test]
    fn builds_sorted_folder_first_tree_from_kernel_note_infos() {
        let tree = build_file_tree_from_notes(vec![
            note("zeta.md"),
            note("lab/notes/b.md"),
            note("lab/a.md"),
            note("alpha.md"),
        ]);

        assert_eq!(tree.len(), 3);
        assert_eq!(tree[0].name, "lab");
        assert!(tree[0].is_folder);
        assert_eq!(tree[0].relative_path, "lab");
        assert_eq!(tree[0].file_count, 2);
        assert_eq!(tree[0].children[0].name, "notes");
        assert_eq!(tree[0].children[0].file_count, 1);
        assert_eq!(tree[0].children[1].relative_path, "lab/a.md");
        assert_eq!(tree[1].relative_path, "alpha.md");
        assert_eq!(tree[2].relative_path, "zeta.md");
    }

    #[test]
    fn normalizes_windows_style_note_paths() {
        let tree = build_file_tree_from_notes(vec![note("folder\\child.md")]);

        assert_eq!(tree[0].relative_path, "folder");
        assert_eq!(tree[0].children[0].relative_path, "folder/child.md");
    }
}
