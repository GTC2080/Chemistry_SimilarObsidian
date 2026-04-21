// Reason: This file owns note delete and rename mutations, reusing the shared lifecycle helper.

#include "storage/storage_note_lifecycle_internal.h"

namespace kernel::storage {

std::error_code mark_note_deleted(Database& db, std::string_view rel_path) {
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  sqlite3_int64 note_id = 0;
  std::error_code ec = detail::lookup_note_id_by_rel_path(db.connection, rel_path, note_id);
  if (ec == std::error_code(SQLITE_DONE, std::generic_category())) {
    return {};
  }
  if (ec) {
    return ec;
  }

  ec = detail::begin_transaction(db.connection);
  if (ec) {
    return ec;
  }

  sqlite3_stmt* stmt = nullptr;
  ec = detail::prepare(
      db.connection,
      "UPDATE notes SET is_deleted=1 WHERE note_id=?1;",
      &stmt);
  if (ec) {
    detail::rollback_transaction(db.connection);
    return ec;
  }
  sqlite3_bind_int64(stmt, 1, note_id);
  ec = detail::finalize_with_result(stmt, sqlite3_step(stmt));
  if (ec) {
    detail::rollback_transaction(db.connection);
    return ec;
  }

  ec = detail::clear_note_parse_rows(db.connection, note_id);
  if (ec) {
    detail::rollback_transaction(db.connection);
    return ec;
  }

  ec = detail::clear_note_attachment_rows(db.connection, note_id);
  if (ec) {
    detail::rollback_transaction(db.connection);
    return ec;
  }

  ec = detail::mark_note_fts_deleted(db.connection, note_id);
  if (ec) {
    detail::rollback_transaction(db.connection);
    return ec;
  }

  ec = detail::commit_transaction(db.connection);
  if (ec) {
    detail::rollback_transaction(db.connection);
  }
  return ec;
}

std::error_code rename_note_metadata(
    Database& db,
    std::string_view old_rel_path,
    std::string_view new_rel_path,
    const kernel::platform::FileStat& stat,
    std::string_view content_revision,
    const kernel::parser::ParseResult& parse_result,
    std::string_view body_text) {
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  sqlite3_int64 note_id = 0;
  std::error_code ec = detail::lookup_note_id_by_rel_path(db.connection, old_rel_path, note_id);
  if (ec == std::error_code(SQLITE_DONE, std::generic_category())) {
    return std::make_error_code(std::errc::no_such_file_or_directory);
  }
  if (ec) {
    return ec;
  }

  sqlite3_int64 conflicting_note_id = 0;
  ec = detail::lookup_note_id_by_rel_path(db.connection, new_rel_path, conflicting_note_id);
  if (!ec && conflicting_note_id != note_id) {
    return std::make_error_code(std::errc::file_exists);
  }
  if (ec && ec != std::error_code(SQLITE_DONE, std::generic_category())) {
    return ec;
  }

  ec = detail::begin_transaction(db.connection);
  if (ec) {
    return ec;
  }

  const std::string title =
      parse_result.title.empty() ? detail::title_from_rel_path(new_rel_path) : parse_result.title;

  sqlite3_stmt* stmt = nullptr;
  ec = detail::prepare(
      db.connection,
      "UPDATE notes SET "
      "  rel_path=?1,"
      "  title=?2,"
      "  file_size=?3,"
      "  mtime_ns=?4,"
      "  content_revision=?5,"
      "  is_deleted=0 "
      "WHERE note_id=?6;",
      &stmt);
  if (ec) {
    detail::rollback_transaction(db.connection);
    return ec;
  }

  sqlite3_bind_text(stmt, 1, std::string(new_rel_path).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, title.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(stat.file_size));
  sqlite3_bind_int64(stmt, 4, static_cast<sqlite3_int64>(stat.mtime_ns));
  sqlite3_bind_text(stmt, 5, std::string(content_revision).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 6, note_id);
  ec = detail::finalize_with_result(stmt, sqlite3_step(stmt));
  if (ec) {
    detail::rollback_transaction(db.connection);
    return ec;
  }

  ec = detail::replace_note_derived_rows(db.connection, note_id, parse_result, title, body_text);
  if (ec) {
    detail::rollback_transaction(db.connection);
    return ec;
  }

  ec = detail::commit_transaction(db.connection);
  if (ec) {
    detail::rollback_transaction(db.connection);
  }
  return ec;
}

}  // namespace kernel::storage
