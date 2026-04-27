use rusqlite::Connection;
use serde::Serialize;

use crate::{sealed_kernel, AppResult};

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
// 查询
// ──────────────────────────────────────────

/// 从 study_sessions 表聚合计算 TruthState
pub fn query_truth_state(conn: &Connection) -> AppResult<TruthStateDto> {
    let mut stmt = conn.prepare(
        "SELECT note_id, COALESCE(SUM(active_secs), 0)
             FROM study_sessions
             GROUP BY note_id",
    )?;

    let rows = stmt.query_map([], |row| {
        Ok((row.get::<_, String>(0)?, row.get::<_, i64>(1)?))
    })?;

    let activities: Vec<(String, i64)> = rows.flatten().collect();
    let state = sealed_kernel::compute_truth_state_from_activity(&activities)?;

    Ok(TruthStateDto {
        level: state.level,
        total_exp: state.total_exp,
        next_level_exp: state.next_level_exp,
        attributes: TruthAttributes {
            science: state.attributes.science,
            engineering: state.attributes.engineering,
            creation: state.attributes.creation,
            finance: state.attributes.finance,
        },
        attribute_exp: TruthAttributes {
            science: state.attribute_exp.science,
            engineering: state.attribute_exp.engineering,
            creation: state.attribute_exp.creation,
            finance: state.attribute_exp.finance,
        },
        last_settlement: super::unix_now_ms()?,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn query_truth_state_delegates_rules_to_kernel() {
        let conn = Connection::open_in_memory().unwrap();
        conn.execute_batch(
            "CREATE TABLE study_sessions (note_id TEXT NOT NULL, active_secs INTEGER NOT NULL);
             INSERT INTO study_sessions (note_id, active_secs) VALUES
             ('lab.csv', 120),
             ('code.rs', 3600),
             ('molecule.mol', 3000),
             ('ledger.base', 6000);",
        )
        .unwrap();

        let state = query_truth_state(&conn).unwrap();

        assert_eq!(state.level, 2);
        assert_eq!(state.total_exp, 112);
        assert_eq!(state.next_level_exp, 150);
        assert_eq!(state.attribute_exp.science, 2);
        assert_eq!(state.attribute_exp.engineering, 60);
        assert_eq!(state.attribute_exp.creation, 50);
        assert_eq!(state.attribute_exp.finance, 100);
        assert_eq!(state.attributes.science, 1);
        assert_eq!(state.attributes.engineering, 2);
        assert_eq!(state.attributes.creation, 2);
        assert_eq!(state.attributes.finance, 3);
    }
}
