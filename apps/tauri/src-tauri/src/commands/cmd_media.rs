use tauri::State;

use crate::models::{MolecularPreview, SpectroscopyData};
use crate::sealed_kernel::{self, SealedKernelState};
use crate::AppError;

#[tauri::command]
pub async fn parse_spectroscopy(
    file_path: String,
    sealed_kernel: State<'_, SealedKernelState>,
) -> Result<SpectroscopyData, AppError> {
    let ext = media_file_extension(&file_path)?;
    let raw = sealed_kernel::read_note_by_file_path(&file_path, sealed_kernel.inner())?;
    tauri::async_runtime::spawn_blocking(move || {
        sealed_kernel::parse_spectroscopy_from_text(&raw, &ext)
    })
    .await
    .map_err(|e| AppError::Custom(format!("线程执行错误: {}", e)))?
}

fn media_file_extension(file_path: &str) -> Result<String, AppError> {
    sealed_kernel::derive_file_extension_from_path(file_path)
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
    let ext = media_file_extension(&file_path)?;
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

#[tauri::command]
pub async fn read_binary_file(
    file_path: String,
    sealed_kernel: State<'_, SealedKernelState>,
) -> Result<Vec<u8>, AppError> {
    let kernel_session = sealed_kernel::active_session_token(sealed_kernel.inner())?;
    tauri::async_runtime::spawn_blocking(move || {
        sealed_kernel::read_vault_file_bytes_for_session(kernel_session, &file_path)
    })
    .await
    .map_err(|e| AppError::Custom(format!("线程执行错误: {}", e)))?
}

#[tauri::command]
pub async fn read_note_indexed_content(
    note_id: String,
    sealed_kernel: State<'_, SealedKernelState>,
) -> Result<String, AppError> {
    sealed_kernel::read_first_changed_markdown_note_content([note_id], sealed_kernel.inner())
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
    fn media_file_extension_uses_kernel_path_rules() {
        assert_eq!(
            media_file_extension("Folder.v1/Spectrum.CSV").expect("kernel file extension"),
            "csv"
        );
        assert_eq!(
            media_file_extension("C:\\vault\\Mol.XYZ").expect("kernel file extension"),
            "xyz"
        );
        assert_eq!(
            media_file_extension("Folder.With.Dot/README").expect("kernel file extension"),
            ""
        );
    }
}
