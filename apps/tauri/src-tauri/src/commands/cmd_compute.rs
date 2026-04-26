use serde::{Deserialize, Serialize};
use std::ffi::{CStr, CString};
use std::os::raw::c_char;

use crate::AppError;

const KERNEL_OK: i32 = 0;
const KERNEL_TRUTH_AWARD_REASON_TEXT_DELTA: i32 = 1;
const KERNEL_TRUTH_AWARD_REASON_CODE_LANGUAGE: i32 = 2;
const KERNEL_TRUTH_AWARD_REASON_MOLECULAR_EDIT: i32 = 3;

#[repr(C)]
#[derive(Debug, Clone, Copy)]
struct KernelStatus {
    code: i32,
}

#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct TruthExpAwardDto {
    pub attr: String,
    pub amount: i32,
    pub reason: String,
}

#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct TruthDiffResultDto {
    pub awards: Vec<TruthExpAwardDto>,
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
struct KernelTruthAward {
    attr: *mut c_char,
    amount: i32,
    reason: i32,
    detail: *mut c_char,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, Default)]
struct KernelTruthDiffResult {
    awards: *mut KernelTruthAward,
    count: usize,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, Default)]
struct KernelOwnedBuffer {
    data: *mut c_char,
    size: usize,
}

fn kernel_string(ptr: *mut c_char) -> String {
    if ptr.is_null() {
        return String::new();
    }
    unsafe { CStr::from_ptr(ptr).to_string_lossy().into_owned() }
}

fn truth_reason_message(reason: i32, detail: &str) -> String {
    match reason {
        KERNEL_TRUTH_AWARD_REASON_TEXT_DELTA => "文本净增量经验".to_string(),
        KERNEL_TRUTH_AWARD_REASON_CODE_LANGUAGE => format!("新增代码块语言: {detail}"),
        KERNEL_TRUTH_AWARD_REASON_MOLECULAR_EDIT => "分子编辑变更".to_string(),
        _ => "内容变更".to_string(),
    }
}

fn truth_awards_from_kernel(
    raw: &KernelTruthDiffResult,
) -> Result<Vec<TruthExpAwardDto>, AppError> {
    if raw.count == 0 {
        return Ok(Vec::new());
    }
    if raw.awards.is_null() {
        return Err(AppError::Custom("Truth diff 内核返回结果无效".to_string()));
    }

    let awards = unsafe { std::slice::from_raw_parts(raw.awards, raw.count) };
    awards
        .iter()
        .map(|award| {
            let attr = kernel_string(award.attr);
            let detail = kernel_string(award.detail);
            Ok(TruthExpAwardDto {
                attr,
                amount: award.amount,
                reason: truth_reason_message(award.reason, &detail),
            })
        })
        .collect()
}

extern "C" {
    fn kernel_compute_truth_diff(
        prev_content: *const c_char,
        prev_size: usize,
        curr_content: *const c_char,
        curr_size: usize,
        file_extension: *const c_char,
        out_result: *mut KernelTruthDiffResult,
    ) -> KernelStatus;

    fn kernel_free_truth_diff_result(result: *mut KernelTruthDiffResult);

    fn kernel_build_semantic_context(
        content: *const c_char,
        content_size: usize,
        out_buffer: *mut KernelOwnedBuffer,
    ) -> KernelStatus;

    fn kernel_free_buffer(buffer: *mut KernelOwnedBuffer);
}

#[tauri::command]
pub fn compute_truth_diff(
    prev_content: String,
    curr_content: String,
    file_extension: String,
) -> Result<TruthDiffResultDto, AppError> {
    let extension = CString::new(file_extension)
        .map_err(|_| AppError::Custom("Truth diff 文件扩展名包含非法字符".to_string()))?;
    let mut result = KernelTruthDiffResult::default();
    let status = unsafe {
        kernel_compute_truth_diff(
            prev_content.as_ptr() as *const c_char,
            prev_content.len(),
            curr_content.as_ptr() as *const c_char,
            curr_content.len(),
            extension.as_ptr(),
            &mut result,
        )
    };
    if status.code != KERNEL_OK {
        unsafe {
            kernel_free_truth_diff_result(&mut result);
        }
        return Err(AppError::Custom("Truth diff 内核计算失败".to_string()));
    }

    let awards = truth_awards_from_kernel(&result);
    unsafe {
        kernel_free_truth_diff_result(&mut result);
    }

    Ok(TruthDiffResultDto { awards: awards? })
}

// ──────────────────────────────────────────
// 语义上下文提取（从前端 JS 迁移到 Rust）
// ──────────────────────────────────────────

#[tauri::command]
pub fn build_semantic_context(content: String) -> String {
    let mut buffer = KernelOwnedBuffer::default();
    let status = unsafe {
        kernel_build_semantic_context(
            content.as_ptr() as *const c_char,
            content.len(),
            &mut buffer,
        )
    };
    if status.code != KERNEL_OK {
        unsafe {
            kernel_free_buffer(&mut buffer);
        }
        return String::new();
    }

    let result = if buffer.size == 0 {
        String::new()
    } else if buffer.data.is_null() {
        String::new()
    } else {
        let bytes = unsafe { std::slice::from_raw_parts(buffer.data as *const u8, buffer.size) };
        String::from_utf8_lossy(bytes).into_owned()
    };

    unsafe {
        kernel_free_buffer(&mut buffer);
    }

    result
}

// ──────────────────────────────────────────
// 化学计量计算（Rust 桥接 C++ kernel）
// ──────────────────────────────────────────

#[repr(C)]
#[derive(Debug, Clone, Copy)]
struct KernelStoichiometryRowInput {
    mw: f64,
    eq: f64,
    moles: f64,
    mass: f64,
    volume: f64,
    density: f64,
    has_density: u8,
    is_reference: u8,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, Default)]
struct KernelStoichiometryRowOutput {
    mw: f64,
    eq: f64,
    moles: f64,
    mass: f64,
    volume: f64,
    density: f64,
    has_density: u8,
    is_reference: u8,
}

extern "C" {
    fn kernel_recalculate_stoichiometry(
        rows: *const KernelStoichiometryRowInput,
        count: usize,
        out_rows: *mut KernelStoichiometryRowOutput,
    ) -> KernelStatus;
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct StoichiometryRow {
    pub id: String,
    pub name: String,
    pub formula: String,
    pub mw: f64,
    pub eq: f64,
    pub moles: f64,
    pub mass: f64,
    pub volume: f64,
    pub is_reference: bool,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub density: Option<f64>,
}

fn kernel_stoichiometry_input_from(row: &StoichiometryRow) -> KernelStoichiometryRowInput {
    KernelStoichiometryRowInput {
        mw: row.mw,
        eq: row.eq,
        moles: row.moles,
        mass: row.mass,
        volume: row.volume,
        density: row.density.unwrap_or(0.0),
        has_density: u8::from(row.density.is_some()),
        is_reference: u8::from(row.is_reference),
    }
}

#[tauri::command]
pub fn recalculate_stoichiometry(rows: Vec<StoichiometryRow>) -> Vec<StoichiometryRow> {
    let input: Vec<KernelStoichiometryRowInput> =
        rows.iter().map(kernel_stoichiometry_input_from).collect();
    let mut output = vec![KernelStoichiometryRowOutput::default(); input.len()];
    let status = unsafe {
        kernel_recalculate_stoichiometry(input.as_ptr(), input.len(), output.as_mut_ptr())
    };
    if status.code != KERNEL_OK {
        return rows;
    }

    rows.into_iter()
        .zip(output)
        .map(|(mut row, out)| {
            row.is_reference = out.is_reference != 0;
            row.eq = out.eq;
            row.moles = out.moles;
            row.mw = out.mw;
            row.mass = out.mass;
            row.volume = out.volume;
            row.density = if out.has_density != 0 {
                Some(out.density)
            } else {
                None
            };
            row
        })
        .collect()
}

// ──────────────────────────────────────────
// 数据库网格归一化（从前端 JS 迁移到 Rust）
// ──────────────────────────────────────────

fn gen_id(prefix: &str) -> String {
    use std::time::{SystemTime, UNIX_EPOCH};
    let nanos = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_nanos();
    format!("{}_{:x}", prefix, nanos & 0xFFFF_FFFF)
}

const ALLOWED_COL_TYPES: &[&str] = &["text", "number", "select", "tags"];

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DatabaseColumn {
    pub id: String,
    pub name: String,
    #[serde(rename = "type")]
    pub col_type: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DatabaseRow {
    pub id: String,
    pub cells: serde_json::Value,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DatabasePayload {
    pub columns: Vec<DatabaseColumn>,
    pub rows: Vec<DatabaseRow>,
}

#[tauri::command]
pub fn normalize_database(input: serde_json::Value) -> DatabasePayload {
    let obj = input.as_object();

    let raw_columns = obj
        .and_then(|o| o.get("columns"))
        .and_then(|v| v.as_array())
        .cloned()
        .unwrap_or_default();

    let columns: Vec<DatabaseColumn> = raw_columns
        .into_iter()
        .filter_map(|v| {
            let obj = v.as_object()?;
            let name = obj
                .get("name")?
                .as_str()
                .unwrap_or("Untitled")
                .trim()
                .to_string();
            let name = if name.is_empty() {
                "Untitled".into()
            } else {
                name
            };
            let id = obj
                .get("id")
                .and_then(|v| v.as_str())
                .map(|s| s.trim().to_string())
                .filter(|s| !s.is_empty())
                .unwrap_or_else(|| gen_id("col"));
            let col_type = obj.get("type").and_then(|v| v.as_str()).unwrap_or("text");
            let col_type = if ALLOWED_COL_TYPES.contains(&col_type) {
                col_type
            } else {
                "text"
            };
            Some(DatabaseColumn {
                id,
                name,
                col_type: col_type.to_string(),
            })
        })
        .collect();

    let safe_columns = if columns.is_empty() {
        vec![
            DatabaseColumn {
                id: gen_id("col"),
                name: "Name".into(),
                col_type: "text".into(),
            },
            DatabaseColumn {
                id: gen_id("col"),
                name: "Tags".into(),
                col_type: "tags".into(),
            },
            DatabaseColumn {
                id: gen_id("col"),
                name: "Notes".into(),
                col_type: "text".into(),
            },
        ]
    } else {
        columns
    };

    let raw_rows = obj
        .and_then(|o| o.get("rows"))
        .and_then(|v| v.as_array())
        .cloned()
        .unwrap_or_default();

    let rows: Vec<DatabaseRow> = raw_rows
        .into_iter()
        .filter_map(|v| {
            let obj = v.as_object()?;
            let id = obj
                .get("id")
                .and_then(|v| v.as_str())
                .map(|s| s.trim().to_string())
                .filter(|s| !s.is_empty())
                .unwrap_or_else(|| gen_id("row"));
            let raw_cells = obj
                .get("cells")
                .and_then(|v| v.as_object())
                .cloned()
                .unwrap_or_default();
            let mut cells = serde_json::Map::new();
            for col in &safe_columns {
                let val = raw_cells
                    .get(&col.id)
                    .cloned()
                    .unwrap_or(serde_json::Value::String(String::new()));
                cells.insert(col.id.clone(), val);
            }
            Some(DatabaseRow {
                id,
                cells: serde_json::Value::Object(cells),
            })
        })
        .collect();

    DatabasePayload {
        columns: safe_columns,
        rows,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn build_semantic_context_uses_kernel_short_trim() {
        let result = build_semantic_context("  short note  \n".to_string());

        assert_eq!(result, "short note");
    }

    #[test]
    fn build_semantic_context_uses_kernel_focus_shape() {
        let content = format!(
            "# Intro\n{}\n\n## Keep 1\nalpha\n\n### Keep 2\nbeta\n\n#### Keep 3\ngamma\n\n# Keep 4\ndelta\n\nfinal block",
            "x".repeat(2300)
        );
        let result = build_semantic_context(content);

        assert_eq!(
            result,
            "Headings:\n## Keep 1\n### Keep 2\n#### Keep 3\n# Keep 4\n\nRecent focus:\n#### Keep 3\ngamma\n\n# Keep 4\ndelta\n\nfinal block"
        );
    }

    #[test]
    fn compute_truth_diff_uses_kernel_code_language_award() {
        let result = compute_truth_diff(
            "note".to_string(),
            "note\n```rust\nfn main() {}\n```".to_string(),
            "md".to_string(),
        )
        .unwrap();

        assert_eq!(result.awards.len(), 1);
        assert_eq!(result.awards[0].attr, "engineering");
        assert_eq!(result.awards[0].amount, 8);
        assert_eq!(result.awards[0].reason, "新增代码块语言: rust");
    }

    #[test]
    fn compute_truth_diff_uses_kernel_molecular_line_award() {
        let result = compute_truth_diff(
            "atom-a".to_string(),
            "atom-a\natom-b\natom-c".to_string(),
            "mol".to_string(),
        )
        .unwrap();

        assert_eq!(result.awards.len(), 1);
        assert_eq!(result.awards[0].attr, "creation");
        assert_eq!(result.awards[0].amount, 10);
        assert_eq!(result.awards[0].reason, "分子编辑变更");
    }

    fn stoich_row(
        id: &str,
        mw: f64,
        eq: f64,
        moles: f64,
        mass: f64,
        volume: f64,
        is_reference: bool,
        density: Option<f64>,
    ) -> StoichiometryRow {
        StoichiometryRow {
            id: id.to_string(),
            name: id.to_string(),
            formula: String::new(),
            mw,
            eq,
            moles,
            mass,
            volume,
            is_reference,
            density,
        }
    }

    #[test]
    fn recalculate_stoichiometry_uses_kernel_reference_row() {
        let result = recalculate_stoichiometry(vec![
            stoich_row("a", 50.0, 2.0, 99.0, 0.0, 0.0, false, None),
            stoich_row("b", 100.0, 7.0, 0.25, 9.0, 3.0, true, Some(2.0)),
            stoich_row("c", 10.0, 3.0, 99.0, 8.0, 4.0, true, None),
        ]);

        assert!(!result[0].is_reference);
        assert!(result[1].is_reference);
        assert!(!result[2].is_reference);
        assert_eq!(result[1].eq, 1.0);
        assert_eq!(result[1].moles, 0.25);
        assert_eq!(result[1].mass, 25.0);
        assert_eq!(result[1].density, Some(2.0));
        assert_eq!(result[1].volume, 12.5);
        assert_eq!(result[2].eq, 3.0);
        assert_eq!(result[2].moles, 0.75);
        assert_eq!(result[2].mass, 7.5);
        assert_eq!(result[2].density, Some(2.0));
        assert_eq!(result[2].volume, 3.75);
    }

    #[test]
    fn recalculate_stoichiometry_defaults_first_row_as_reference() {
        let result = recalculate_stoichiometry(vec![
            stoich_row("a", 20.0, 4.0, 0.5, 0.0, 0.0, false, Some(5.0)),
            stoich_row("b", 30.0, 2.0, 7.0, 0.0, 0.0, false, None),
        ]);

        assert!(result[0].is_reference);
        assert_eq!(result[0].eq, 1.0);
        assert_eq!(result[0].moles, 0.5);
        assert_eq!(result[0].mass, 10.0);
        assert_eq!(result[0].volume, 2.0);
        assert!(!result[1].is_reference);
        assert_eq!(result[1].moles, 1.0);
        assert_eq!(result[1].mass, 30.0);
    }

    #[test]
    fn recalculate_stoichiometry_delegates_empty_rows_to_kernel() {
        let result = recalculate_stoichiometry(Vec::new());

        assert!(result.is_empty());
    }
}
