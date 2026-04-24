use rusqlite::{params, Connection};
use serde::Serialize;

use crate::AppResult;

// ──────────────────────────────────────────
// 数据结构
// ──────────────────────────────────────────

#[derive(Debug, Serialize)]
pub struct DailySummary {
    pub date: String,
    pub active_secs: i64,
    pub file_count: i64,
}

#[derive(Debug, Serialize)]
pub struct DailyFileDetail {
    pub note_id: String,
    pub active_secs: i64,
}

#[derive(Debug, Serialize)]
pub struct DailyDetailGroup {
    pub date: String,
    pub files: Vec<DailyFileDetail>,
}

#[derive(Debug, Serialize)]
pub struct FolderRank {
    pub folder: String,
    pub total_secs: i64,
}

#[derive(Debug, Serialize)]
pub struct HeatmapDay {
    pub date: String,
    pub active_secs: i64,
}

#[derive(Debug, Serialize)]
pub struct StudyStats {
    pub today_active_secs: i64,
    pub today_files: i64,
    pub week_active_secs: i64,
    pub streak_days: i64,
    pub daily_summary: Vec<DailySummary>,
    pub daily_details: Vec<DailyDetailGroup>,
    pub folder_ranking: Vec<FolderRank>,
    pub heatmap: Vec<HeatmapDay>,
}

// ──────────────────────────────────────────
// 子查询
// ──────────────────────────────────────────

fn query_today(conn: &Connection, today_start: i64) -> AppResult<(i64, i64)> {
    conn.query_row(
        "SELECT COALESCE(SUM(active_secs), 0), COUNT(DISTINCT note_id)
         FROM study_sessions WHERE started_at >= ?1",
        params![today_start],
        |row| Ok((row.get(0)?, row.get(1)?)),
    )
    .map_err(Into::into)
}

fn query_week(conn: &Connection, week_start: i64) -> AppResult<i64> {
    conn.query_row(
        "SELECT COALESCE(SUM(active_secs), 0)
         FROM study_sessions WHERE started_at >= ?1",
        params![week_start],
        |row| row.get(0),
    )
    .map_err(Into::into)
}

fn query_streak(conn: &Connection, today_bucket: i64) -> AppResult<i64> {
    let mut stmt = conn.prepare(
        "SELECT DISTINCT (started_at / 86400) AS day_bucket
             FROM study_sessions
             WHERE active_secs > 0
             ORDER BY day_bucket DESC",
    )?;

    let buckets: Vec<i64> = stmt
        .query_map([], |row| row.get(0))?
        .filter_map(|r| r.ok())
        .collect();

    let mut streak: i64 = 0;
    let mut expected = today_bucket;
    for bucket in buckets {
        if bucket == expected {
            streak += 1;
            expected -= 1;
        } else if bucket < expected {
            break;
        }
    }
    Ok(streak)
}

fn query_daily_summary(conn: &Connection, window_start: i64) -> AppResult<Vec<DailySummary>> {
    let mut stmt = conn.prepare(
        "SELECT date(started_at, 'unixepoch') AS d,
                    COALESCE(SUM(active_secs), 0),
                    COUNT(DISTINCT note_id)
             FROM study_sessions
             WHERE started_at >= ?1
             GROUP BY d ORDER BY d DESC",
    )?;

    let rows = stmt
        .query_map(params![window_start], |row| {
            Ok(DailySummary {
                date: row.get(0)?,
                active_secs: row.get(1)?,
                file_count: row.get(2)?,
            })
        })?
        .filter_map(|r| r.ok())
        .collect();
    Ok(rows)
}

fn query_daily_details(conn: &Connection, window_start: i64) -> AppResult<Vec<DailyDetailGroup>> {
    let mut stmt = conn.prepare(
        "SELECT date(started_at, 'unixepoch') AS d,
                    note_id,
                    COALESCE(SUM(active_secs), 0)
             FROM study_sessions
             WHERE started_at >= ?1
             GROUP BY d, note_id
             ORDER BY d DESC, SUM(active_secs) DESC",
    )?;

    let rows: Vec<(String, String, i64)> = stmt
        .query_map(params![window_start], |row| {
            Ok((row.get(0)?, row.get(1)?, row.get(2)?))
        })?
        .filter_map(|r| r.ok())
        .collect();

    let mut groups: Vec<DailyDetailGroup> = Vec::new();
    for (date, note_id, active_secs) in rows {
        if let Some(g) = groups.last_mut().filter(|g| g.date == date) {
            g.files.push(DailyFileDetail {
                note_id,
                active_secs,
            });
        } else {
            groups.push(DailyDetailGroup {
                date,
                files: vec![DailyFileDetail {
                    note_id,
                    active_secs,
                }],
            });
        }
    }
    Ok(groups)
}

fn query_folder_ranking(conn: &Connection) -> AppResult<Vec<FolderRank>> {
    let mut stmt = conn.prepare(
        "SELECT folder, COALESCE(SUM(active_secs), 0) AS total
             FROM study_sessions
             GROUP BY folder ORDER BY total DESC LIMIT 5",
    )?;

    let rows = stmt
        .query_map([], |row| {
            Ok(FolderRank {
                folder: row.get(0)?,
                total_secs: row.get(1)?,
            })
        })?
        .filter_map(|r| r.ok())
        .collect();
    Ok(rows)
}

fn query_heatmap(conn: &Connection, heatmap_start: i64) -> AppResult<Vec<HeatmapDay>> {
    let mut stmt = conn.prepare(
        "SELECT date(started_at, 'unixepoch') AS d,
                    COALESCE(SUM(active_secs), 0)
             FROM study_sessions
             WHERE started_at >= ?1
             GROUP BY d ORDER BY d ASC",
    )?;

    let rows = stmt
        .query_map(params![heatmap_start], |row| {
            Ok(HeatmapDay {
                date: row.get(0)?,
                active_secs: row.get(1)?,
            })
        })?
        .filter_map(|r| r.ok())
        .collect();
    Ok(rows)
}

// ──────────────────────────────────────────
// 热力图网格预计算（从前端 JS 迁移到 Rust）
// ──────────────────────────────────────────

#[derive(Debug, Serialize)]
pub struct HeatmapCell {
    pub date: String,
    pub secs: i64,
    pub col: usize,
    pub row: usize,
}

#[derive(Debug, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct HeatmapGrid {
    pub cells: Vec<HeatmapCell>,
    pub max_secs: i64,
}

/// 日期格式化（epoch 秒 → "YYYY-MM-DD"），无需外部 crate
fn format_date_from_epoch(epoch_secs: i64) -> String {
    // Howard Hinnant's date algorithm
    let days = epoch_secs / 86400;
    let z = days + 719468;
    let era = (if z >= 0 { z } else { z - 146096 }) / 146097;
    let doe = (z - era * 146097) as u64;
    let yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    let y = yoe as i64 + era * 400;
    let doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    let mp = (5 * doy + 2) / 153;
    let d = doy - (153 * mp + 2) / 5 + 1;
    let m = if mp < 10 { mp + 3 } else { mp - 9 };
    let y = if m <= 2 { y + 1 } else { y };
    format!("{:04}-{:02}-{:02}", y, m, d)
}

/// 返回 26 周 x 7 天的预计算热力图网格
pub fn query_heatmap_cells(conn: &Connection) -> AppResult<HeatmapGrid> {
    const WEEKS: usize = 26;
    const DAYS_PER_WEEK: usize = 7;

    let now_secs = super::unix_now_secs()?;
    let today_start = (now_secs / 86400) * 86400;

    // 拉取原始热力图数据
    let heatmap_start = today_start - (WEEKS as i64 * DAYS_PER_WEEK as i64) * 86400;
    let raw = query_heatmap(conn, heatmap_start)?;
    let map: std::collections::HashMap<String, i64> =
        raw.into_iter().map(|h| (h.date, h.active_secs)).collect();

    let total_days = WEEKS * DAYS_PER_WEEK;
    let mut start_date = today_start - (total_days as i64 - 1) * 86400;

    // 对齐到周一: 1970-01-01 是周四 (weekday index 3, 0=Mon)
    // days_since_epoch % 7: 0=Thu, 要得到 0=Mon 需 +3 再 %7
    let day_of_week = ((start_date / 86400 % 7) + 3).rem_euclid(7); // 0=Mon
    start_date -= day_of_week * 86400;

    let mut cells = Vec::with_capacity(WEEKS * DAYS_PER_WEEK);
    let mut max_secs: i64 = 0;

    for w in 0..WEEKS {
        for d in 0..DAYS_PER_WEEK {
            let ts = start_date + (w * 7 + d) as i64 * 86400;
            let date = format_date_from_epoch(ts);
            let secs = map.get(&date).copied().unwrap_or(0);
            if secs > max_secs {
                max_secs = secs;
            }
            cells.push(HeatmapCell {
                date,
                secs,
                col: w,
                row: d,
            });
        }
    }

    Ok(HeatmapGrid { cells, max_secs })
}

// ──────────────────────────────────────────
// 聚合入口
// ──────────────────────────────────────────

/// 聚合统计，days_back 控制 daily_summary / daily_details 回溯天数
pub fn query_stats(conn: &Connection, days_back: i64) -> AppResult<StudyStats> {
    let now_secs = super::unix_now_secs()?;
    let today_start = (now_secs / 86400) * 86400;

    let (today_active_secs, today_files) = query_today(conn, today_start)?;
    let week_active_secs = query_week(conn, today_start - 6 * 86400)?;
    let streak_days = query_streak(conn, today_start / 86400)?;

    let window_start = today_start - (days_back - 1) * 86400;
    let daily_summary = query_daily_summary(conn, window_start)?;
    let daily_details = query_daily_details(conn, window_start)?;
    let folder_ranking = query_folder_ranking(conn)?;
    let heatmap = query_heatmap(conn, today_start - 179 * 86400)?;

    Ok(StudyStats {
        today_active_secs,
        today_files,
        week_active_secs,
        streak_days,
        daily_summary,
        daily_details,
        folder_ranking,
        heatmap,
    })
}
