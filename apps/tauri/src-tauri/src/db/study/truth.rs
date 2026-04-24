use rusqlite::Connection;
use serde::Serialize;
use std::cmp;

use crate::AppResult;

// ──────────────────────────────────────────
// 数据结构
// ──────────────────────────────────────────

#[derive(Debug, Serialize)]
pub struct TruthAttributes {
    pub science: i64,
    pub engineering: i64,
    pub creation: i64,
    pub finance: i64,
}

#[derive(Debug, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct TruthStateDto {
    pub level: i64,
    pub total_exp: i64,
    pub next_level_exp: i64,
    pub attributes: TruthAttributes,
    pub attribute_exp: TruthAttributes,
    pub last_settlement: i64,
}

// ──────────────────────────────────────────
// 常量与辅助
// ──────────────────────────────────────────

/// 1 EXP = 60 秒有效学习时长
const SECS_PER_EXP: f64 = 60.0;
const BASE_EXP: f64 = 100.0;
const GROWTH_RATE: f64 = 1.5;
const ATTR_EXP_PER_LEVEL: i64 = 50;

/// 根据 note_id 的文件扩展名路由到四个属性之一
fn route_note_to_attr(note_id: &str) -> &'static str {
    let ext = note_id
        .rsplit('.')
        .next()
        .unwrap_or("")
        .to_ascii_lowercase();
    match ext.as_str() {
        "jdx" | "csv" => "science",
        "py" | "js" | "ts" | "tsx" | "jsx" | "rs" | "go" | "c" | "cpp" | "java" => "engineering",
        "mol" | "chemdraw" => "creation",
        "dashboard" | "base" => "finance",
        _ => "creation",
    }
}

fn calc_next_level_exp(level: i64) -> i64 {
    (BASE_EXP * GROWTH_RATE.powi((level - 1) as i32)).floor() as i64
}

fn attr_level(exp: i64) -> i64 {
    cmp::min(99, 1 + exp / ATTR_EXP_PER_LEVEL)
}

fn secs_to_exp(secs: i64) -> i64 {
    (secs as f64 / SECS_PER_EXP).floor() as i64
}

// ──────────────────────────────────────────
// 查询
// ──────────────────────────────────────────

/// 从 study_sessions 表聚合计算 TruthState
pub fn query_truth_state(conn: &Connection) -> AppResult<TruthStateDto> {
    let mut stmt = conn.prepare(
        "SELECT note_id, COALESCE(SUM(active_secs), 0)
             FROM study_sessions
             GROUP BY note_id",
    )?;

    let mut secs = [0i64; 4]; // [science, engineering, creation, finance]

    let rows = stmt.query_map([], |row| {
        Ok((row.get::<_, String>(0)?, row.get::<_, i64>(1)?))
    })?;

    for row in rows.flatten() {
        let (note_id, s) = row;
        let idx = match route_note_to_attr(&note_id) {
            "science" => 0,
            "engineering" => 1,
            "finance" => 3,
            _ => 2, // creation
        };
        secs[idx] += s;
    }

    let exp: [i64; 4] = std::array::from_fn(|i| secs_to_exp(secs[i]));
    let grand_total: i64 = exp.iter().sum();

    // 等级计算
    let mut level: i64 = 1;
    let mut remaining = grand_total;
    let mut next_level_exp = calc_next_level_exp(level);

    while remaining >= next_level_exp {
        remaining -= next_level_exp;
        level += 1;
        next_level_exp = calc_next_level_exp(level);
    }

    Ok(TruthStateDto {
        level,
        total_exp: remaining,
        next_level_exp,
        attributes: TruthAttributes {
            science: attr_level(exp[0]),
            engineering: attr_level(exp[1]),
            creation: attr_level(exp[2]),
            finance: attr_level(exp[3]),
        },
        attribute_exp: TruthAttributes {
            science: exp[0],
            engineering: exp[1],
            creation: exp[2],
            finance: exp[3],
        },
        last_settlement: super::unix_now_ms()?,
    })
}
