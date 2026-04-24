use regex::Regex;
use serde::{Deserialize, Serialize};
use std::collections::HashSet;
use std::sync::OnceLock;

use crate::AppError;

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

fn code_block_regex() -> &'static Regex {
    static RE: OnceLock<Regex> = OnceLock::new();
    RE.get_or_init(|| Regex::new(r"```(\w+)").expect("代码块正则编译失败"))
}

fn route_by_extension(ext: &str) -> &'static str {
    let lower = ext.to_ascii_lowercase();
    if ["jdx", "csv"].contains(&lower.as_str()) {
        return "science";
    }
    if [
        "py", "js", "ts", "tsx", "jsx", "rs", "go", "c", "cpp", "java",
    ]
    .contains(&lower.as_str())
    {
        return "engineering";
    }
    if ["mol", "chemdraw"].contains(&lower.as_str()) {
        return "creation";
    }
    if ["dashboard", "base"].contains(&lower.as_str()) {
        return "finance";
    }
    "creation"
}

fn route_by_code_language(lang: &str) -> Option<&'static str> {
    let lower = lang.to_ascii_lowercase();
    if [
        "python",
        "py",
        "rust",
        "go",
        "javascript",
        "js",
        "typescript",
        "ts",
        "java",
        "c",
        "cpp",
    ]
    .contains(&lower.as_str())
    {
        return Some("engineering");
    }
    if ["smiles", "chemical", "latex", "math"].contains(&lower.as_str()) {
        return Some("science");
    }
    if ["sql", "r", "stata"].contains(&lower.as_str()) {
        return Some("finance");
    }
    None
}

fn extract_code_languages(content: &str) -> HashSet<String> {
    code_block_regex()
        .captures_iter(content)
        .filter_map(|cap| cap.get(1).map(|m| m.as_str().to_ascii_lowercase()))
        .collect()
}

#[tauri::command]
pub fn compute_truth_diff(
    prev_content: String,
    curr_content: String,
    file_extension: String,
) -> Result<TruthDiffResultDto, AppError> {
    const EXP_PER_100_CHARS: i32 = 2;
    const EXP_PER_MOL_EDIT: i32 = 5;
    const EXP_PER_CODE_BLOCK: i32 = 8;

    if prev_content.is_empty() || curr_content.is_empty() {
        return Ok(TruthDiffResultDto { awards: Vec::new() });
    }

    let mut awards = Vec::new();
    let delta = curr_content.len() as i32 - prev_content.len() as i32;
    if delta > 10 {
        let char_exp = ((delta as f64 / 100.0) * EXP_PER_100_CHARS as f64).floor() as i32;
        if char_exp > 0 {
            awards.push(TruthExpAwardDto {
                attr: route_by_extension(&file_extension).to_string(),
                amount: char_exp,
                reason: "文本净增量经验".to_string(),
            });
        }
    }

    let new_blocks = extract_code_languages(&curr_content);
    let old_blocks = extract_code_languages(&prev_content);
    for lang in new_blocks.difference(&old_blocks) {
        if let Some(attr) = route_by_code_language(lang) {
            awards.push(TruthExpAwardDto {
                attr: attr.to_string(),
                amount: EXP_PER_CODE_BLOCK,
                reason: format!("新增代码块语言: {}", lang),
            });
        }
    }

    if file_extension.eq_ignore_ascii_case("mol") || file_extension.eq_ignore_ascii_case("chemdraw")
    {
        let prev_lines = prev_content.lines().count();
        let curr_lines = curr_content.lines().count();
        if curr_lines > prev_lines {
            awards.push(TruthExpAwardDto {
                attr: "creation".to_string(),
                amount: (curr_lines - prev_lines) as i32 * EXP_PER_MOL_EDIT,
                reason: "分子编辑变更".to_string(),
            });
        }
    }

    Ok(TruthDiffResultDto { awards })
}

// ──────────────────────────────────────────
// 语义上下文提取（从前端 JS 迁移到 Rust）
// ──────────────────────────────────────────

const MIN_CONTEXT_CHARS: usize = 24;
const MAX_CONTEXT_CHARS: usize = 2200;

fn trim_to_max(text: &str) -> &str {
    if text.len() <= MAX_CONTEXT_CHARS {
        text
    } else {
        &text[text.len() - MAX_CONTEXT_CHARS..]
    }
}

#[tauri::command]
pub fn build_semantic_context(content: String) -> String {
    let trimmed = content.trim();
    if trimmed.len() <= MAX_CONTEXT_CHARS {
        return trimmed.to_string();
    }

    let lines: Vec<&str> = trimmed.lines().collect();
    let headings: Vec<&str> = lines
        .iter()
        .filter(|l| {
            let t = l.trim_start();
            t.starts_with("# ")
                || t.starts_with("## ")
                || t.starts_with("### ")
                || t.starts_with("#### ")
        })
        .copied()
        .rev()
        .take(4)
        .collect::<Vec<_>>()
        .into_iter()
        .rev()
        .collect();

    let blocks: Vec<&str> = trimmed
        .split("\n\n")
        .map(|b| b.trim())
        .filter(|b| !b.is_empty())
        .collect();
    let recent_blocks: Vec<&str> = blocks
        .iter()
        .rev()
        .take(3)
        .copied()
        .collect::<Vec<_>>()
        .into_iter()
        .rev()
        .collect();

    let mut sections = Vec::new();
    if !headings.is_empty() {
        sections.push(format!("Headings:\n{}", headings.join("\n")));
    }
    if !recent_blocks.is_empty() {
        sections.push(format!("Recent focus:\n{}", recent_blocks.join("\n\n")));
    }
    let joined = sections.join("\n\n");

    if joined.len() >= MIN_CONTEXT_CHARS {
        trim_to_max(&joined).to_string()
    } else {
        trim_to_max(trimmed).to_string()
    }
}

// ──────────────────────────────────────────
// 化学计量计算（从前端 JS 迁移到 Rust）
// ──────────────────────────────────────────

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

fn to_non_negative(v: f64) -> f64 {
    if v.is_finite() && v > 0.0 {
        v
    } else {
        0.0
    }
}

#[tauri::command]
pub fn recalculate_stoichiometry(rows: Vec<StoichiometryRow>) -> Vec<StoichiometryRow> {
    if rows.is_empty() {
        return rows;
    }

    let reference_index = rows.iter().position(|r| r.is_reference).unwrap_or(0);
    let reference_moles = to_non_negative(rows[reference_index].moles);

    rows.into_iter()
        .enumerate()
        .map(|(i, mut row)| {
            let is_ref = i == reference_index;
            let eq = if is_ref { 1.0 } else { to_non_negative(row.eq) };
            let moles = if is_ref {
                reference_moles
            } else {
                reference_moles * eq
            };
            let mw = to_non_negative(row.mw);
            let mass = moles * mw;

            let inferred_density = if row.density.map_or(false, |d| d > 0.0) {
                row.density
            } else if row.mass > 0.0 && row.volume > 0.0 {
                Some(row.mass / row.volume)
            } else {
                None
            };

            let volume = inferred_density
                .filter(|d| *d > 0.0)
                .map(|d| mass / d)
                .unwrap_or(0.0);

            row.is_reference = is_ref;
            row.eq = eq;
            row.moles = moles;
            row.mw = mw;
            row.mass = mass;
            row.volume = volume;
            row.density = inferred_density;
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
