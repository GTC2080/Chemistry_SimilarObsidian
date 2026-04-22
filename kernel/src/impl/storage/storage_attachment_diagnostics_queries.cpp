// Reason: This file owns attachment diagnostics/support-bundle counts so
// attachment public-surface read queries can stay focused on catalog lookups.

#include "storage/storage_internal.h"

namespace kernel::storage {

namespace {

sqlite3_int64 normalize_limit(const std::size_t limit) {
  return limit == static_cast<std::size_t>(-1)
             ? static_cast<sqlite3_int64>(-1)
             : static_cast<sqlite3_int64>(limit);
}

constexpr char kLiveAttachmentCountSql[] =
    "SELECT COUNT(*) "
    "FROM ("
    "  SELECT DISTINCT note_attachment_refs.attachment_rel_path "
    "  FROM note_attachment_refs "
    "  JOIN notes ON notes.note_id = note_attachment_refs.note_id "
    "  WHERE notes.is_deleted = 0"
    ");";

constexpr char kMissingLiveAttachmentCountSql[] =
    "SELECT COUNT(*) "
    "FROM ("
    "  SELECT DISTINCT note_attachment_refs.attachment_rel_path "
    "  FROM note_attachment_refs "
    "  JOIN notes ON notes.note_id = note_attachment_refs.note_id "
    "  JOIN attachments ON attachments.rel_path = note_attachment_refs.attachment_rel_path "
    "  WHERE notes.is_deleted = 0 AND attachments.is_missing = 1"
    ");";

constexpr char kOrphanedAttachmentCountSql[] =
    "SELECT COUNT(*) "
    "FROM attachments "
    "LEFT JOIN ("
    "  SELECT DISTINCT note_attachment_refs.attachment_rel_path "
    "  FROM note_attachment_refs "
    "  JOIN notes ON notes.note_id = note_attachment_refs.note_id "
    "  WHERE notes.is_deleted = 0"
    ") AS live_refs "
    "  ON live_refs.attachment_rel_path = attachments.rel_path "
    "WHERE live_refs.attachment_rel_path IS NULL;";

constexpr char kMissingLiveAttachmentPathSummarySql[] =
    "SELECT DISTINCT note_attachment_refs.attachment_rel_path "
    "FROM note_attachment_refs "
    "JOIN notes ON notes.note_id = note_attachment_refs.note_id "
    "JOIN attachments ON attachments.rel_path = note_attachment_refs.attachment_rel_path "
    "WHERE notes.is_deleted = 0 AND attachments.is_missing = 1 "
    "ORDER BY note_attachment_refs.attachment_rel_path ASC "
    "LIMIT ?1;";

constexpr char kOrphanedAttachmentPathSummarySql[] =
    "SELECT attachments.rel_path "
    "FROM attachments "
    "LEFT JOIN ("
    "  SELECT DISTINCT note_attachment_refs.attachment_rel_path "
    "  FROM note_attachment_refs "
    "  JOIN notes ON notes.note_id = note_attachment_refs.note_id "
    "  WHERE notes.is_deleted = 0"
    ") AS live_refs "
    "  ON live_refs.attachment_rel_path = attachments.rel_path "
    "WHERE live_refs.attachment_rel_path IS NULL "
    "ORDER BY attachments.rel_path ASC "
    "LIMIT ?1;";

std::error_code list_attachment_path_summary(
    sqlite3* db,
    const char* sql,
    const std::size_t limit,
    std::vector<std::string>& out_paths) {
  out_paths.clear();
  sqlite3_stmt* stmt = nullptr;
  std::error_code ec = detail::prepare(db, sql, &stmt);
  if (ec) {
    return ec;
  }

  sqlite3_bind_int64(stmt, 1, normalize_limit(limit));
  return detail::append_statement_rows(stmt, [&](sqlite3_stmt* row) {
    std::string path;
    detail::assign_text_column(row, 0, path);
    out_paths.push_back(std::move(path));
  });
}

}  // namespace

std::error_code count_attachments(Database& db, std::uint64_t& out_count) {
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  return detail::scalar_count_query(db.connection, kLiveAttachmentCountSql, out_count);
}

std::error_code count_missing_attachments(Database& db, std::uint64_t& out_count) {
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  return detail::scalar_count_query(db.connection, kMissingLiveAttachmentCountSql, out_count);
}

std::error_code count_orphaned_attachments(Database& db, std::uint64_t& out_count) {
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  return detail::scalar_count_query(db.connection, kOrphanedAttachmentCountSql, out_count);
}

std::error_code list_missing_attachment_paths(
    Database& db,
    const std::size_t limit,
    std::vector<std::string>& out_paths) {
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  return list_attachment_path_summary(
      db.connection,
      kMissingLiveAttachmentPathSummarySql,
      limit,
      out_paths);
}

std::error_code list_orphaned_attachment_paths(
    Database& db,
    const std::size_t limit,
    std::vector<std::string>& out_paths) {
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  return list_attachment_path_summary(
      db.connection,
      kOrphanedAttachmentPathSummarySql,
      limit,
      out_paths);
}

}  // namespace kernel::storage
