use std::ffi::{CStr, CString};
use std::fs;
use std::os::raw::c_char;
use std::path::Path;

use tauri::State;

use crate::models::SpectroscopyData;
use crate::sealed_kernel::{self, SealedKernelState};
use crate::services::spectroscopy::parse_spectroscopy_from_text;
use crate::shared::command_utils::{is_molecular_extension, is_spectroscopy_extension};
use crate::AppError;

const DEFAULT_PREVIEW_ATOM_LIMIT: usize = 2000;
const MIN_PREVIEW_ATOM_LIMIT: usize = 200;
const MAX_PREVIEW_ATOM_LIMIT: usize = 20000;
const KERNEL_OK: i32 = 0;
const KERNEL_MOLECULAR_PREVIEW_ERROR_UNSUPPORTED_EXTENSION: i32 = 1;

#[derive(Debug, Clone, serde::Serialize)]
pub struct MolecularPreview {
    pub preview_data: String,
    pub atom_count: usize,
    pub preview_atom_count: usize,
    pub truncated: bool,
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
struct KernelStatus {
    code: i32,
}

#[repr(C)]
#[derive(Debug)]
struct KernelMolecularPreview {
    preview_data: *mut c_char,
    atom_count: usize,
    preview_atom_count: usize,
    truncated: u8,
    error: i32,
}

impl Default for KernelMolecularPreview {
    fn default() -> Self {
        Self {
            preview_data: std::ptr::null_mut(),
            atom_count: 0,
            preview_atom_count: 0,
            truncated: 0,
            error: 0,
        }
    }
}

extern "C" {
    fn kernel_build_molecular_preview(
        raw: *const c_char,
        raw_size: usize,
        extension: *const c_char,
        max_atoms: usize,
        out_preview: *mut KernelMolecularPreview,
    ) -> KernelStatus;
    fn kernel_free_molecular_preview(preview: *mut KernelMolecularPreview);
}

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

    if !is_spectroscopy_extension(&ext) {
        return Err(AppError::Custom(format!("不支持的波谱文件扩展名: {}", ext)));
    }

    let raw = sealed_kernel::read_note_by_file_path(&file_path, sealed_kernel.inner())?;
    tauri::async_runtime::spawn_blocking(move || {
        parse_spectroscopy_from_text(&raw, &ext).map_err(Into::into)
    })
    .await
    .map_err(|e| AppError::Custom(format!("线程执行错误: {}", e)))?
}

fn clamp_preview_limit(limit: Option<usize>) -> usize {
    limit
        .unwrap_or(DEFAULT_PREVIEW_ATOM_LIMIT)
        .clamp(MIN_PREVIEW_ATOM_LIMIT, MAX_PREVIEW_ATOM_LIMIT)
}

fn molecular_preview_error_message(error: i32, extension: &str) -> String {
    match error {
        KERNEL_MOLECULAR_PREVIEW_ERROR_UNSUPPORTED_EXTENSION => {
            format!("不支持的分子文件扩展名: {}", extension)
        }
        _ => "分子预览内核构建失败".to_string(),
    }
}

unsafe fn molecular_preview_from_kernel(
    raw: &KernelMolecularPreview,
) -> Result<MolecularPreview, String> {
    if raw.preview_data.is_null() {
        return Err("分子预览内核缺少 preview_data 输出".to_string());
    }
    Ok(MolecularPreview {
        preview_data: CStr::from_ptr(raw.preview_data)
            .to_string_lossy()
            .into_owned(),
        atom_count: raw.atom_count,
        preview_atom_count: raw.preview_atom_count,
        truncated: raw.truncated != 0,
    })
}

fn build_molecular_preview(
    raw: &str,
    extension: &str,
    max_atoms: usize,
) -> Result<MolecularPreview, AppError> {
    let extension_c = CString::new(extension)
        .map_err(|_| AppError::Custom("分子文件扩展名包含非法空字符".to_string()))?;
    let mut result = KernelMolecularPreview::default();
    let status = unsafe {
        kernel_build_molecular_preview(
            raw.as_ptr() as *const c_char,
            raw.len(),
            extension_c.as_ptr(),
            max_atoms,
            &mut result,
        )
    };
    if status.code != KERNEL_OK {
        let message = molecular_preview_error_message(result.error, extension);
        unsafe { kernel_free_molecular_preview(&mut result) };
        return Err(AppError::Custom(message));
    }

    let preview = unsafe { molecular_preview_from_kernel(&result) }.map_err(AppError::Custom);
    unsafe { kernel_free_molecular_preview(&mut result) };
    preview
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
    if !is_molecular_extension(&ext) {
        return Err(AppError::Custom(format!("不支持的分子文件扩展名: {}", ext)));
    }

    let raw = sealed_kernel::read_note_by_file_path(&file_path, sealed_kernel.inner())?;
    let limit = clamp_preview_limit(max_atoms);
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
