// Reason: This file owns note-oriented read queries and journal mirror writes.

#include "storage/storage_internal.h"

namespace kernel::storage {

namespace {

NoteListHit read_note_list_hit(sqlite3_stmt* stmt) {
  NoteListHit hit{};
  detail::assign_text_column(stmt, 0, hit.rel_path);
  detail::assign_text_column(stmt, 1, hit.title);
  return hit;
}

}  // namespace

std::error_code list_note_paths(Database& db, std::vector<std::string>& out_paths) {
  out_paths.clear();
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  sqlite3_stmt* stmt = nullptr;
  std::error_code ec = detail::prepare(
      db.connection,
      "SELECT rel_path FROM notes WHERE is_deleted=0 ORDER BY rel_path ASC;",
      &stmt);
  if (ec) {
    return ec;
  }

  return detail::append_statement_rows(stmt, [&](sqlite3_stmt* row) {
    std::string path;
    detail::assign_text_column(row, 0, path);
    out_paths.push_back(std::move(path));
  });
}

std::error_code count_active_notes(Database& db, std::uint64_t& out_count) {
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }
  return detail::scalar_count_query(
      db.connection,
      "SELECT COUNT(*) FROM notes WHERE is_deleted=0;",
      out_count);
}

std::error_code list_notes_by_tag(
    Database& db,
    std::string_view tag,
    const std::size_t limit,
    std::vector<NoteListHit>& out_hits) {
  out_hits.clear();
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  sqlite3_stmt* stmt = nullptr;
  std::error_code ec = detail::prepare(
      db.connection,
      "SELECT notes.rel_path, notes.title "
      "FROM note_tags "
      "JOIN notes ON notes.note_id = note_tags.note_id "
      "WHERE note_tags.tag = ?1 AND notes.is_deleted = 0 "
      "ORDER BY notes.rel_path ASC "
      "LIMIT ?2;",
      &stmt);
  if (ec) {
    return ec;
  }

  sqlite3_bind_text(stmt, 1, std::string(tag).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(limit));
  return detail::append_statement_rows(stmt, [&](sqlite3_stmt* row) {
    out_hits.push_back(read_note_list_hit(row));
  });
}

std::error_code list_backlinks_for_rel_path(
    Database& db,
    std::string_view rel_path,
    const std::size_t limit,
    std::vector<NoteListHit>& out_hits) {
  out_hits.clear();
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  const std::string fallback_title = detail::title_from_rel_path(rel_path);

  sqlite3_stmt* stmt = nullptr;
  std::error_code ec = detail::prepare(
      db.connection,
      "SELECT src.rel_path, src.title "
      "FROM notes AS dst "
      "JOIN note_links ON note_links.target = dst.title OR note_links.target = ?2 "
      "JOIN notes AS src ON src.note_id = note_links.note_id "
      "WHERE dst.rel_path = ?1 AND dst.is_deleted = 0 AND src.is_deleted = 0 "
      "GROUP BY src.note_id, src.rel_path, src.title "
      "ORDER BY src.rel_path ASC "
      "LIMIT ?3;",
      &stmt);
  if (ec) {
    return ec;
  }

  sqlite3_bind_text(stmt, 1, std::string(rel_path).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, fallback_title.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(limit));
  return detail::append_statement_rows(stmt, [&](sqlite3_stmt* row) {
    out_hits.push_back(read_note_list_hit(row));
  });
}

std::error_code insert_journal_state(
    Database& db,
    std::string_view op_id,
    std::string_view op_type,
    std::string_view rel_path,
    std::string_view temp_path,
    std::string_view phase) {
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  sqlite3_stmt* stmt = nullptr;
  std::error_code ec = detail::prepare(
      db.connection,
      "INSERT INTO journal_state(op_id, op_type, rel_path, temp_path, phase, recorded_at_ns) "
      "VALUES(?1, ?2, ?3, ?4, ?5, ?6);",
      &stmt);
  if (ec) {
    return ec;
  }

  sqlite3_bind_text(stmt, 1, std::string(op_id).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, std::string(op_type).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, std::string(rel_path).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, std::string(temp_path).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 5, std::string(phase).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 6, static_cast<sqlite3_int64>(detail::now_ns()));
  return detail::finalize_with_result(stmt, sqlite3_step(stmt));
}

}  // namespace kernel::storage
