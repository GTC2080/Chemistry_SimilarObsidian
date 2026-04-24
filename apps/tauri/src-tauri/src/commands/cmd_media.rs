use std::fs;
use std::path::Path;

use tauri::State;

use crate::db::{self, DbState};
use crate::models::SpectroscopyData;
use crate::sealed_kernel::{self, SealedKernelState};
use crate::services::spectroscopy::parse_spectroscopy_from_text;
use crate::shared::command_utils::{is_molecular_extension, is_spectroscopy_extension};
use crate::AppError;

const DEFAULT_PREVIEW_ATOM_LIMIT: usize = 2000;
const MIN_PREVIEW_ATOM_LIMIT: usize = 200;
const MAX_PREVIEW_ATOM_LIMIT: usize = 20000;

#[derive(Debug, Clone, serde::Serialize)]
pub struct MolecularPreview {
    pub preview_data: String,
    pub atom_count: usize,
    pub preview_atom_count: usize,
    pub truncated: bool,
}

#[tauri::command]
pub async fn parse_spectroscopy(file_path: String) -> Result<SpectroscopyData, AppError> {
    tauri::async_runtime::spawn_blocking(move || {
        let ext = Path::new(&file_path)
            .extension()
            .and_then(|e| e.to_str())
            .unwrap_or("")
            .to_lowercase();

        if !is_spectroscopy_extension(&ext) {
            return Err(AppError::Custom(format!("不支持的波谱文件扩展名: {}", ext)));
        }

        let raw = read_note_sync(&file_path)?;
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

fn build_pdb_preview(raw: &str, max_atoms: usize) -> MolecularPreview {
    let mut atom_count = 0usize;
    let mut preview_atom_count = 0usize;
    let mut lines: Vec<String> = Vec::new();

    for line in raw.lines() {
        let is_atom_line = line.starts_with("ATOM") || line.starts_with("HETATM");
        if is_atom_line {
            atom_count += 1;
            if preview_atom_count < max_atoms {
                preview_atom_count += 1;
                lines.push(line.to_string());
            }
            continue;
        }
        lines.push(line.to_string());
    }

    MolecularPreview {
        preview_data: lines.join("\n"),
        atom_count,
        preview_atom_count: preview_atom_count.min(atom_count),
        truncated: atom_count > max_atoms,
    }
}

fn build_xyz_preview(raw: &str, max_atoms: usize) -> MolecularPreview {
    let mut lines = raw.lines();
    let _header_count = lines.next();
    let comment = lines.next().unwrap_or("");
    let atom_lines: Vec<&str> = lines.collect();
    let atom_count = atom_lines
        .iter()
        .filter(|line| !line.trim().is_empty())
        .count();
    let preview_atom_count = atom_count.min(max_atoms);

    let mut preview_lines: Vec<String> = Vec::with_capacity(preview_atom_count + 2);
    preview_lines.push(preview_atom_count.to_string());
    preview_lines.push(comment.to_string());
    preview_lines.extend(
        atom_lines
            .iter()
            .filter(|line| !line.trim().is_empty())
            .take(preview_atom_count)
            .map(|line| (*line).to_string()),
    );

    MolecularPreview {
        preview_data: preview_lines.join("\n"),
        atom_count,
        preview_atom_count,
        truncated: atom_count > max_atoms,
    }
}

fn build_cif_preview(raw: &str) -> MolecularPreview {
    MolecularPreview {
        preview_data: raw.to_string(),
        atom_count: 0,
        preview_atom_count: 0,
        truncated: false,
    }
}

#[tauri::command]
pub async fn read_molecular_preview(
    file_path: String,
    max_atoms: Option<usize>,
) -> Result<MolecularPreview, AppError> {
    tauri::async_runtime::spawn_blocking(move || {
        let ext = Path::new(&file_path)
            .extension()
            .and_then(|e| e.to_str())
            .unwrap_or("")
            .to_lowercase();
        if !is_molecular_extension(&ext) {
            return Err(AppError::Custom(format!("不支持的分子文件扩展名: {}", ext)));
        }

        let raw = read_note_sync(&file_path)?;
        let limit = clamp_preview_limit(max_atoms);

        let preview = match ext.as_str() {
            "pdb" => build_pdb_preview(&raw, limit),
            "xyz" => build_xyz_preview(&raw, limit),
            _ => build_cif_preview(&raw),
        };

        Ok(preview)
    })
    .await
    .map_err(|e| AppError::Custom(format!("线程执行错误: {}", e)))?
}

/// 同步读取文件内容（内部使用，被其他命令复用）
fn read_note_sync(file_path: &str) -> Result<String, AppError> {
    let path = Path::new(file_path);
    if !path.exists() {
        return Err(AppError::Custom(format!("文件不存在: {}", file_path)));
    }
    if !path.is_file() {
        return Err(AppError::Custom(format!(
            "指定路径不是一个文件: {}",
            file_path
        )));
    }
    let bytes = fs::read(path)?;

    if bytes.len() >= 2 {
        if bytes[0] == 0xFF && bytes[1] == 0xFE {
            let u16s: Vec<u16> = bytes[2..]
                .chunks_exact(2)
                .map(|c| u16::from_le_bytes([c[0], c[1]]))
                .collect();
            return String::from_utf16(&u16s).map_err(|e| {
                AppError::Custom(format!("UTF-16 LE 解码失败 [{}]: {}", file_path, e))
            });
        }
        if bytes[0] == 0xFE && bytes[1] == 0xFF {
            let u16s: Vec<u16> = bytes[2..]
                .chunks_exact(2)
                .map(|c| u16::from_be_bytes([c[0], c[1]]))
                .collect();
            return String::from_utf16(&u16s).map_err(|e| {
                AppError::Custom(format!("UTF-16 BE 解码失败 [{}]: {}", file_path, e))
            });
        }
    }

    // 尝试零拷贝 UTF-8 转换，失败时再 lossy 解码
    match String::from_utf8(bytes) {
        Ok(s) => Ok(s),
        Err(e) => Ok(String::from_utf8_lossy(e.as_bytes()).into_owned()),
    }
}

#[tauri::command]
pub fn read_note(
    file_path: String,
    sealed_kernel: State<'_, SealedKernelState>,
) -> Result<String, AppError> {
    sealed_kernel::read_note_by_file_path(&file_path, sealed_kernel.inner())
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
    db: State<'_, DbState>,
) -> Result<String, AppError> {
    let db_conn = db.conn.clone();
    tauri::async_runtime::spawn_blocking(move || {
        let conn = db_conn.lock().map_err(|_| AppError::Lock)?;
        let content = db::get_note_content_by_id(&conn, &note_id)?.unwrap_or_default();
        Ok(content)
    })
    .await
    .map_err(|e| AppError::Custom(format!("线程执行错误: {}", e)))?
}
