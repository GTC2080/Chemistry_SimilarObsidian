//! PDF 相关命令：文件读取 + 批注持久化
//!
//! 渲染、文本提取、搜索、目录等功能已迁移至前端 pdf.js，
//! Rust 端仅负责文件 I/O 和批注存储。

use std::path::PathBuf;

use tauri::{ipc::Response, State};

use crate::error::{AppError, AppResult};
use crate::pdf::annotations::PdfAnnotation;
use crate::pdf::ink::{RawStroke, SmoothedStroke};
use crate::sealed_kernel::{self, SealedKernelState};

/// 读取 PDF 文件的原始字节，通过 IPC Response 返回（零 JSON 序列化开销）。
/// 前端直接得到 ArrayBuffer，可传给 pdf.js 的 `data` 参数。
#[tauri::command]
pub async fn read_pdf_file(file_path: String) -> Result<Response, AppError> {
    let bytes = tokio::fs::read(&file_path)
        .await
        .map_err(|e| AppError::PdfEngine(format!("读取 PDF 失败: {file_path} — {e}")))?;
    Ok(Response::new(bytes))
}

/// 加载指定 PDF 文件的批注列表
#[tauri::command]
pub async fn load_pdf_annotations(
    vault_path: String,
    file_path: String,
    sealed_kernel: State<'_, SealedKernelState>,
) -> AppResult<Vec<PdfAnnotation>> {
    sealed_kernel::ensure_vault_open(&vault_path, sealed_kernel.inner())?;
    let pdf_rel_path = pdf_rel_path_from_file_path(&file_path, sealed_kernel.inner())?;
    let vault_root = PathBuf::from(vault_path);

    tokio::task::spawn_blocking(move || {
        crate::pdf::annotations::load_annotations(&vault_root, &pdf_rel_path)
    })
    .await
    .map_err(|e| AppError::PdfAnnotation(format!("spawn_blocking 失败: {e}")))?
}

fn pdf_rel_path_from_file_path(file_path: &str, state: &SealedKernelState) -> AppResult<String> {
    sealed_kernel::relativize_vault_path(file_path, false, state).map_err(|_| {
        AppError::PdfAnnotation(format!("PDF 不在当前 vault 内或路径非法: {file_path}"))
    })
}

/// 笔迹平滑（CPU 密集算法在后端执行）：Douglas-Peucker 简化 + Catmull-Rom 插值
#[tauri::command]
pub async fn smooth_ink_strokes(
    strokes: Vec<RawStroke>,
    tolerance: Option<f32>,
) -> AppResult<Vec<SmoothedStroke>> {
    let tol = match tolerance {
        Some(value) => value,
        None => sealed_kernel::pdf_ink_default_tolerance()?,
    };
    let result = tokio::task::spawn_blocking(move || crate::pdf::ink::smooth_strokes(strokes, tol))
        .await
        .map_err(|e| AppError::PdfAnnotation(format!("spawn_blocking 失败: {e}")))?;
    result.map_err(AppError::PdfAnnotation)
}

/// 保存 PDF 批注列表到磁盘
#[tauri::command]
pub async fn save_pdf_annotations(
    vault_path: String,
    file_path: String,
    annotations_data: Vec<PdfAnnotation>,
    sealed_kernel: State<'_, SealedKernelState>,
) -> AppResult<()> {
    sealed_kernel::ensure_vault_open(&vault_path, sealed_kernel.inner())?;
    let pdf_rel_path = pdf_rel_path_from_file_path(&file_path, sealed_kernel.inner())?;
    let vault_root = PathBuf::from(vault_path);
    let pdf_path = PathBuf::from(file_path);

    tokio::task::spawn_blocking(move || {
        crate::pdf::annotations::save_annotations(
            &vault_root,
            &pdf_path,
            &pdf_rel_path,
            annotations_data,
        )
    })
    .await
    .map_err(|e| AppError::PdfAnnotation(format!("spawn_blocking 失败: {e}")))?
}
