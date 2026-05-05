//! PDF 批注持久化模块
//!
//! 批注以 JSON 文件形式存储，文件路径与读写由 kernel 拥有。
//!
//! 文件内容同时记录 kernel 计算的 PDF 文件内容哈希，可用于检测文件是否被替换。

use serde::{Deserialize, Serialize};

use crate::error::{AppError, AppResult};
use crate::sealed_kernel;

// ---------------------------------------------------------------------------
// 数据结构
// ---------------------------------------------------------------------------

/// PDF 页面上的一个矩形区域（归一化坐标，相对页面宽高）
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct Rect {
    pub x: f32,
    pub y: f32,
    pub w: f32,
    pub h: f32,
}

/// 文本偏移区间及其对应的页面矩形列表（用于高亮批注）
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct TextRange {
    pub start_offset: u32,
    pub end_offset: u32,
    pub rects: Vec<Rect>,
}

/// 手绘笔迹中的单个坐标点（归一化坐标 + 压感）
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct InkPoint {
    pub x: f32,
    pub y: f32,
    /// 压感值 0.0–1.0（无压感设备默认 0.5）
    #[serde(default = "default_pressure")]
    pub pressure: f32,
}

fn default_pressure() -> f32 {
    0.5
}

/// 一条连续笔画（pen-down → pen-up 之间的点集合）
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct InkStroke {
    /// 归一化坐标点序列
    pub points: Vec<InkPoint>,
    /// 笔画宽度（归一化，相对页面宽度）
    pub stroke_width: f32,
}

/// 单条 PDF 批注
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct PdfAnnotation {
    /// 唯一标识符（UUID 字符串）
    pub id: String,
    /// 0-based 页码
    pub page_number: u32,
    /// 批注类型："highlight" | "note" | "area" | "ink"
    #[serde(rename = "type")]
    pub annotation_type: String,
    /// 颜色（CSS 颜色字符串，如 "#FFFF00"）
    pub color: String,
    /// 文本高亮区间（highlight 类型使用）
    #[serde(skip_serializing_if = "Option::is_none")]
    pub text_ranges: Option<Vec<TextRange>>,
    /// 区域批注矩形（area 类型使用）
    #[serde(skip_serializing_if = "Option::is_none")]
    pub area: Option<Rect>,
    /// 批注正文内容（note / area 类型使用）
    #[serde(skip_serializing_if = "Option::is_none")]
    pub content: Option<String>,
    /// 高亮所选文本的原始字符串
    #[serde(skip_serializing_if = "Option::is_none")]
    pub selected_text: Option<String>,
    /// 手绘笔画列表（ink 类型使用）
    #[serde(skip_serializing_if = "Option::is_none")]
    pub ink_strokes: Option<Vec<InkStroke>>,
    /// 创建时间（ISO 8601 字符串）
    pub created_at: String,
    /// 最后修改时间（ISO 8601 字符串）
    pub updated_at: String,
}

/// 单个 PDF 文件对应的批注存储文件结构
#[derive(Debug, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct AnnotationFile {
    /// PDF 文件路径（相对于 vault 根目录）
    pub pdf_path: String,
    /// PDF 文件内容哈希（首尾各 1KB + 文件大小 → SHA-256 十六进制）
    pub pdf_hash: String,
    /// 所有批注列表
    pub annotations: Vec<PdfAnnotation>,
}

/// 加载指定 PDF 的批注列表；若文件不存在则返回空列表
pub fn load_annotations_for_session(
    session: usize,
    pdf_rel_path: &str,
) -> AppResult<Vec<PdfAnnotation>> {
    let raw = sealed_kernel::read_pdf_annotation_json_for_session(session, pdf_rel_path)?;
    if raw.is_empty() {
        return Ok(Vec::new());
    }

    let af: AnnotationFile = serde_json::from_str(&raw)
        .map_err(|e| AppError::PdfAnnotation(format!("解析批注 JSON 失败: {e}")))?;

    Ok(af.annotations)
}

/// 将批注列表持久化到磁盘。
pub fn save_annotations_with_hash_for_session(
    session: usize,
    pdf_rel_path: &str,
    pdf_hash: String,
    annotations: Vec<PdfAnnotation>,
) -> AppResult<()> {
    let af = AnnotationFile {
        pdf_path: pdf_rel_path.to_string(),
        pdf_hash,
        annotations,
    };

    let json = serde_json::to_string_pretty(&af)
        .map_err(|e| AppError::PdfAnnotation(format!("序列化批注 JSON 失败: {e}")))?;

    sealed_kernel::write_pdf_annotation_json_for_session(session, pdf_rel_path, &json)
}
