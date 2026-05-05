// Reason: This file owns the kernel-side study/session persistence queries
// that replace the legacy Tauri Rust index.db study table.

#include "storage/storage_internal.h"

namespace kernel::storage {

namespace {

void bind_text(sqlite3_stmt* stmt, const int index, std::string_view value) {
  sqlite3_bind_text(stmt, index, std::string(value).c_str(), -1, SQLITE_TRANSIENT);
}

std::error_code step_done(sqlite3_stmt* stmt) {
  return detail::finalize_with_result(stmt, sqlite3_step(stmt));
}

std::error_code query_i64_pair(
    sqlite3* db,
    const char* sql,
    const std::int64_t bind_value,
    std::int64_t& first,
    std::int64_t& second) {
  sqlite3_stmt* stmt = nullptr;
  std::error_code ec = detail::prepare(db, sql, &stmt);
  if (ec) {
    return ec;
  }
  sqlite3_bind_int64(stmt, 1, bind_value);
  const int step_rc = sqlite3_step(stmt);
  if (step_rc != SQLITE_ROW) {
    sqlite3_finalize(stmt);
    return std::error_code(step_rc, std::generic_category());
  }
  first = sqlite3_column_int64(stmt, 0);
  second = sqlite3_column_int64(stmt, 1);
  return detail::finalize_with_result(stmt, step_rc);
}

std::error_code query_i64(
    sqlite3* db,
    const char* sql,
    const std::int64_t bind_value,
    std::int64_t& out_value) {
  sqlite3_stmt* stmt = nullptr;
  std::error_code ec = detail::prepare(db, sql, &stmt);
  if (ec) {
    return ec;
  }
  sqlite3_bind_int64(stmt, 1, bind_value);
  const int step_rc = sqlite3_step(stmt);
  if (step_rc != SQLITE_ROW) {
    sqlite3_finalize(stmt);
    return std::error_code(step_rc, std::generic_category());
  }
  out_value = sqlite3_column_int64(stmt, 0);
  return detail::finalize_with_result(stmt, step_rc);
}

}  // namespace

std::error_code start_study_session(
    Database& db,
    std::string_view note_id,
    std::string_view folder,
    const std::int64_t started_at_epoch_secs,
    std::int64_t& out_session_id) {
  if (db.connection == nullptr || note_id.empty()) {
    return std::make_error_code(std::errc::invalid_argument);
  }

  sqlite3_stmt* stmt = nullptr;
  std::error_code ec = detail::prepare(
      db.connection,
      "INSERT INTO study_sessions(note_id, folder, started_at, active_secs) "
      "VALUES(?1, ?2, ?3, 0);",
      &stmt);
  if (ec) {
    return ec;
  }
  bind_text(stmt, 1, note_id);
  bind_text(stmt, 2, folder);
  sqlite3_bind_int64(stmt, 3, started_at_epoch_secs);
  ec = step_done(stmt);
  if (ec) {
    return ec;
  }

  out_session_id = static_cast<std::int64_t>(sqlite3_last_insert_rowid(db.connection));
  return {};
}

std::error_code add_study_session_active_secs(
    Database& db,
    const std::int64_t session_id,
    const std::int64_t active_secs) {
  if (db.connection == nullptr || session_id <= 0 || active_secs < 0) {
    return std::make_error_code(std::errc::invalid_argument);
  }

  sqlite3_stmt* stmt = nullptr;
  std::error_code ec = detail::prepare(
      db.connection,
      "UPDATE study_sessions SET active_secs = active_secs + ?1 WHERE id = ?2;",
      &stmt);
  if (ec) {
    return ec;
  }
  sqlite3_bind_int64(stmt, 1, active_secs);
  sqlite3_bind_int64(stmt, 2, session_id);
  return step_done(stmt);
}

std::error_code query_study_stats_records(
    Database& db,
    const std::int64_t today_start_epoch_secs,
    const std::int64_t week_start_epoch_secs,
    const std::int64_t daily_window_start_epoch_secs,
    const std::int64_t heatmap_start_epoch_secs,
    const std::size_t folder_rank_limit,
    StudyStatsRecords& out_records) {
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::invalid_argument);
  }

  out_records = StudyStatsRecords{};
  std::error_code ec = query_i64_pair(
      db.connection,
      "SELECT COALESCE(SUM(active_secs), 0), COUNT(DISTINCT note_id) "
      "FROM study_sessions WHERE started_at >= ?1;",
      today_start_epoch_secs,
      out_records.today_active_secs,
      out_records.today_files);
  if (ec) {
    return ec;
  }

  ec = query_i64(
      db.connection,
      "SELECT COALESCE(SUM(active_secs), 0) "
      "FROM study_sessions WHERE started_at >= ?1;",
      week_start_epoch_secs,
      out_records.week_active_secs);
  if (ec) {
    return ec;
  }

  sqlite3_stmt* stmt = nullptr;
  ec = detail::prepare(
      db.connection,
      "SELECT started_at FROM study_sessions "
      "WHERE active_secs > 0 ORDER BY started_at DESC;",
      &stmt);
  if (ec) {
    return ec;
  }
  ec = detail::append_statement_rows(stmt, [&](sqlite3_stmt* row) {
    out_records.streak_started_at_epoch_secs.push_back(sqlite3_column_int64(row, 0));
  });
  if (ec) {
    return ec;
  }

  ec = detail::prepare(
      db.connection,
      "SELECT date(started_at, 'unixepoch') AS d, "
      "       COALESCE(SUM(active_secs), 0), "
      "       COUNT(DISTINCT note_id) "
      "FROM study_sessions WHERE started_at >= ?1 "
      "GROUP BY d ORDER BY d DESC;",
      &stmt);
  if (ec) {
    return ec;
  }
  sqlite3_bind_int64(stmt, 1, daily_window_start_epoch_secs);
  ec = detail::append_statement_rows(stmt, [&](sqlite3_stmt* row) {
    StudyDailySummaryRecord record{};
    detail::assign_text_column(row, 0, record.date);
    record.active_secs = sqlite3_column_int64(row, 1);
    record.file_count = sqlite3_column_int64(row, 2);
    out_records.daily_summary.push_back(std::move(record));
  });
  if (ec) {
    return ec;
  }

  ec = detail::prepare(
      db.connection,
      "SELECT date(started_at, 'unixepoch') AS d, "
      "       note_id, folder, COALESCE(SUM(active_secs), 0) "
      "FROM study_sessions WHERE started_at >= ?1 "
      "GROUP BY d, note_id, folder "
      "ORDER BY d DESC, SUM(active_secs) DESC;",
      &stmt);
  if (ec) {
    return ec;
  }
  sqlite3_bind_int64(stmt, 1, daily_window_start_epoch_secs);
  ec = detail::append_statement_rows(stmt, [&](sqlite3_stmt* row) {
    StudyDailyDetailRecord record{};
    detail::assign_text_column(row, 0, record.date);
    detail::assign_text_column(row, 1, record.note_id);
    detail::assign_text_column(row, 2, record.folder);
    record.active_secs = sqlite3_column_int64(row, 3);
    out_records.daily_details.push_back(std::move(record));
  });
  if (ec) {
    return ec;
  }

  ec = detail::prepare(
      db.connection,
      "SELECT folder, COALESCE(SUM(active_secs), 0) AS total "
      "FROM study_sessions GROUP BY folder ORDER BY total DESC LIMIT ?1;",
      &stmt);
  if (ec) {
    return ec;
  }
  sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(folder_rank_limit));
  ec = detail::append_statement_rows(stmt, [&](sqlite3_stmt* row) {
    StudyFolderRankRecord record{};
    detail::assign_text_column(row, 0, record.folder);
    record.total_secs = sqlite3_column_int64(row, 1);
    out_records.folder_ranking.push_back(std::move(record));
  });
  if (ec) {
    return ec;
  }

  ec = detail::prepare(
      db.connection,
      "SELECT date(started_at, 'unixepoch') AS d, COALESCE(SUM(active_secs), 0) "
      "FROM study_sessions WHERE started_at >= ?1 "
      "GROUP BY d ORDER BY d ASC;",
      &stmt);
  if (ec) {
    return ec;
  }
  sqlite3_bind_int64(stmt, 1, heatmap_start_epoch_secs);
  return detail::append_statement_rows(stmt, [&](sqlite3_stmt* row) {
    StudyHeatmapDayRecord record{};
    detail::assign_text_column(row, 0, record.date);
    record.active_secs = sqlite3_column_int64(row, 1);
    out_records.heatmap.push_back(std::move(record));
  });
}

std::error_code query_study_note_activity_records(
    Database& db,
    std::vector<StudyNoteActivityRecord>& out_records) {
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::invalid_argument);
  }

  out_records.clear();
  sqlite3_stmt* stmt = nullptr;
  std::error_code ec = detail::prepare(
      db.connection,
      "SELECT note_id, COALESCE(SUM(active_secs), 0) "
      "FROM study_sessions GROUP BY note_id;",
      &stmt);
  if (ec) {
    return ec;
  }
  return detail::append_statement_rows(stmt, [&](sqlite3_stmt* row) {
    StudyNoteActivityRecord record{};
    detail::assign_text_column(row, 0, record.note_id);
    record.active_secs = sqlite3_column_int64(row, 1);
    out_records.push_back(std::move(record));
  });
}

std::error_code query_all_study_heatmap_day_records(
    Database& db,
    std::vector<StudyHeatmapDayRecord>& out_records) {
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::invalid_argument);
  }

  out_records.clear();
  sqlite3_stmt* stmt = nullptr;
  std::error_code ec = detail::prepare(
      db.connection,
      "SELECT date(started_at, 'unixepoch') AS d, COALESCE(SUM(active_secs), 0) "
      "FROM study_sessions GROUP BY d ORDER BY d ASC;",
      &stmt);
  if (ec) {
    return ec;
  }
  return detail::append_statement_rows(stmt, [&](sqlite3_stmt* row) {
    StudyHeatmapDayRecord record{};
    detail::assign_text_column(row, 0, record.date);
    record.active_secs = sqlite3_column_int64(row, 1);
    out_records.push_back(std::move(record));
  });
}

}  // namespace kernel::storage
