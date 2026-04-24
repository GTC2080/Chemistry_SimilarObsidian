use crate::models::{FileTreeNode, NoteInfo};

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

#[tauri::command]
pub fn build_file_tree(notes: Vec<NoteInfo>) -> Vec<FileTreeNode> {
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
