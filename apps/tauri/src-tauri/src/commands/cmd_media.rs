use std::fs;
use std::path::Path;

use tauri::State;

use crate::models::{MolecularPreview, SpectroscopyData};
use crate::sealed_kernel::{self, SealedKernelState};
use crate::AppError;

#[tauri::command]
pub async fn parse_spectroscopy(
    file_path: String,
    sealed_kernel: State<'_, SealedKernelState>,
) -> Result<SpectroscopyData, AppError> {
    let ext = Path::new(&file_path)
        .extension()
        .and_then(|e| e.to_str())
        .unwrap_or("")
        .to_lowercase();

    let raw = sealed_kernel::read_note_by_file_path(&file_path, sealed_kernel.inner())?;
    tauri::async_runtime::spawn_blocking(move || {
        sealed_kernel::parse_spectroscopy_from_text(&raw, &ext)
    })
    .await
    .map_err(|e| AppError::Custom(format!("线程执行错误: {}", e)))?
}

fn normalize_preview_limit(limit: Option<usize>) -> Result<usize, AppError> {
    sealed_kernel::normalize_molecular_preview_atom_limit(limit.unwrap_or(0))
}

fn build_molecular_preview(
    raw: &str,
    extension: &str,
    max_atoms: usize,
) -> Result<MolecularPreview, AppError> {
    sealed_kernel::build_molecular_preview_from_text(raw, extension, max_atoms)
}

#[tauri::command]
pub async fn read_molecular_preview(
    file_path: String,
    max_atoms: Option<usize>,
    sealed_kernel: State<'_, SealedKernelState>,
) -> Result<MolecularPreview, AppError> {
    let ext = Path::new(&file_path)
        .extension()
        .and_then(|e| e.to_str())
        .unwrap_or("")
        .to_lowercase();
    let raw = sealed_kernel::read_note_by_file_path(&file_path, sealed_kernel.inner())?;
    let limit = normalize_preview_limit(max_atoms)?;
    tauri::async_runtime::spawn_blocking(move || build_molecular_preview(&raw, &ext, limit))
        .await
        .map_err(|e| AppError::Custom(format!("线程执行错误: {}", e)))?
}

#[tauri::command]
pub fn read_note(
    file_path: String,
    sealed_kernel: State<'_, SealedKernelState>,
) -> Result<String, AppError> {
    sealed_kernel::read_note_by_file_path(&file_path, sealed_kernel.inner())
}

fn indexed_markdown_rel_path(note_id: &str) -> Result<Option<String>, AppError> {
    let rel_paths = sealed_kernel::filter_changed_markdown_paths(&[note_id.to_string()])?;
    Ok(rel_paths.into_iter().next())
}

#[tauri::command]
pub async fn read_binary_file(file_path: String) -> Result<Vec<u8>, AppError> {
    tauri::async_runtime::spawn_blocking(move || {
        let path = Path::new(&file_path);
        if !path.exists() {
            return Err(AppError::Custom(format!("文件不存在: {}", file_path)));
        }
        if !path.is_file() {
            return Err(AppError::Custom(format!(
                "指定路径不是一个文件: {}",
                file_path
            )));
        }
        fs::read(path).map_err(Into::into)
    })
    .await
    .map_err(|e| AppError::Custom(format!("线程执行错误: {}", e)))?
}

#[tauri::command]
pub async fn read_note_indexed_content(
    note_id: String,
    sealed_kernel: State<'_, SealedKernelState>,
) -> Result<String, AppError> {
    let Some(rel_path) = indexed_markdown_rel_path(&note_id)? else {
        return Ok(String::new());
    };

    sealed_kernel::read_note_by_rel_path(&rel_path, sealed_kernel.inner())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn molecular_preview_uses_kernel_for_pdb_truncation() {
        let raw = "HEADER sample\nATOM one\nHETATM two\nATOM three\nEND\n";
        let preview = build_molecular_preview(raw, "pdb", 2).expect("kernel molecular preview");

        assert_eq!(preview.atom_count, 3);
        assert_eq!(preview.preview_atom_count, 2);
        assert!(preview.truncated);
        assert!(preview.preview_data.contains("HEADER sample"));
        assert!(preview.preview_data.contains("HETATM two"));
        assert!(!preview.preview_data.contains("ATOM three"));
    }

    #[test]
    fn molecular_preview_uses_kernel_for_xyz_header() {
        let raw = "4\ncomment\nO 0 0 0\n\nH 0 1 0\nH 0 -1 0\n";
        let preview = build_molecular_preview(raw, "xyz", 2).expect("kernel molecular preview");

        assert_eq!(preview.atom_count, 3);
        assert_eq!(preview.preview_atom_count, 2);
        assert!(preview.truncated);
        assert!(preview.preview_data.starts_with("2\ncomment\n"));
        assert!(!preview.preview_data.contains("H 0 -1 0"));
    }

    #[test]
    fn molecular_preview_delegates_extension_support_to_kernel() {
        let error =
            build_molecular_preview("raw text", "txt", 2).expect_err("unsupported extension");

        assert_eq!(error.to_string(), "不支持的分子文件扩展名: txt".to_string());
    }

    #[test]
    fn indexed_markdown_rel_path_uses_kernel_path_filter() {
        assert_eq!(
            indexed_markdown_rel_path(" Folder\\Note.md ").expect("kernel markdown path filter"),
            Some("Folder/Note.md".to_string())
        );
        assert_eq!(
            indexed_markdown_rel_path("Folder/Note.txt").expect("kernel markdown path filter"),
            None
        );
    }
}
