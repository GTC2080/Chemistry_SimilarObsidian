mod session;
mod stats;
mod truth;

pub use session::{end_session, start_session, tick_session};
pub use stats::{query_heatmap_cells, query_stats, HeatmapGrid, StudyStats};
pub use truth::{query_truth_state, TruthStateDto};

// ──────────────────────────────────────────
// 共享时间工具
// ──────────────────────────────────────────

use std::time::{SystemTime, UNIX_EPOCH};

use crate::AppResult;

fn unix_now_secs() -> AppResult<i64> {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_secs() as i64)
        .map_err(Into::into)
}

fn unix_now_ms() -> AppResult<i64> {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_millis() as i64)
        .map_err(Into::into)
}
