// Reason: This file owns attachment metadata state transitions only.

#include "storage/storage_internal.h"

namespace kernel::storage {

std::error_code upsert_attachment_metadata(
    Database& db,
    std::string_view rel_path,
    const kernel::platform::FileStat& stat) {
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  sqlite3_stmt* stmt = nullptr;
  std::error_code ec = detail::prepare(
      db.connection,
      "INSERT INTO attachments(rel_path, file_size, mtime_ns, is_missing) "
      "VALUES(?1, ?2, ?3, 0) "
      "ON CONFLICT(rel_path) DO UPDATE SET "
      "  file_size=excluded.file_size,"
      "  mtime_ns=excluded.mtime_ns,"
      "  is_missing=0;",
      &stmt);
  if (ec) {
    return ec;
  }

  sqlite3_bind_text(stmt, 1, std::string(rel_path).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(stat.file_size));
  sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(stat.mtime_ns));
  return detail::finalize_with_result(stmt, sqlite3_step(stmt));
}

std::error_code mark_attachment_missing(Database& db, std::string_view rel_path) {
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  sqlite3_stmt* stmt = nullptr;
  std::error_code ec = detail::prepare(
      db.connection,
      "INSERT INTO attachments(rel_path, file_size, mtime_ns, is_missing) "
      "VALUES(?1, 0, 0, 1) "
      "ON CONFLICT(rel_path) DO UPDATE SET "
      "  is_missing=1;",
      &stmt);
  if (ec) {
    return ec;
  }

  sqlite3_bind_text(stmt, 1, std::string(rel_path).c_str(), -1, SQLITE_TRANSIENT);
  return detail::finalize_with_result(stmt, sqlite3_step(stmt));
}

}  // namespace kernel::storage
