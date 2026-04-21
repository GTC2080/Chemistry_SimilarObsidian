// Reason: This file owns note upsert mutations and the shared derived-row replacement helper.

#include "storage/storage_note_lifecycle_internal.h"

namespace kernel::storage::detail {

std::error_code replace_note_derived_rows(
    sqlite3* db,
    const sqlite3_int64 note_id,
    const kernel::parser::ParseResult& parse_result,
    std::string_view title,
    std::string_view body_text) {
  std::error_code ec = clear_note_parse_rows(db, note_id);
  if (ec) {
    return ec;
  }

  ec = clear_note_attachment_rows(db, note_id);
  if (ec) {
    return ec;
  }

  for (const auto& tag : parse_result.tags) {
    ec = insert_note_tag(db, note_id, tag);
    if (ec) {
      return ec;
    }
  }

  for (const auto& link : parse_result.wikilinks) {
    ec = insert_note_link(db, note_id, link);
    if (ec) {
      return ec;
    }
  }

  for (const auto& attachment_ref : parse_result.attachment_refs) {
    ec = insert_note_attachment_ref(db, note_id, attachment_ref);
    if (ec) {
      return ec;
    }
  }

  return replace_note_fts_row(db, note_id, title, body_text);
}

}  // namespace kernel::storage::detail

namespace kernel::storage {

std::error_code upsert_note_metadata(
    Database& db,
    std::string_view rel_path,
    const kernel::platform::FileStat& stat,
    std::string_view content_revision,
    const kernel::parser::ParseResult& parse_result,
    std::string_view body_text) {
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  std::error_code ec = detail::begin_transaction(db.connection);
  if (ec) {
    return ec;
  }

  const std::string title =
      parse_result.title.empty() ? detail::title_from_rel_path(rel_path) : parse_result.title;

  sqlite3_stmt* stmt = nullptr;
  ec = detail::prepare(
      db.connection,
      "INSERT INTO notes(rel_path, title, file_size, mtime_ns, content_revision, is_deleted) "
      "VALUES(?1, ?2, ?3, ?4, ?5, 0) "
      "ON CONFLICT(rel_path) DO UPDATE SET "
      "  title=excluded.title,"
      "  file_size=excluded.file_size,"
      "  mtime_ns=excluded.mtime_ns,"
      "  content_revision=excluded.content_revision,"
      "  is_deleted=0;",
      &stmt);
  if (ec) {
    detail::rollback_transaction(db.connection);
    return ec;
  }

  sqlite3_bind_text(stmt, 1, std::string(rel_path).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, title.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(stat.file_size));
  sqlite3_bind_int64(stmt, 4, static_cast<sqlite3_int64>(stat.mtime_ns));
  sqlite3_bind_text(stmt, 5, std::string(content_revision).c_str(), -1, SQLITE_TRANSIENT);
  ec = detail::finalize_with_result(stmt, sqlite3_step(stmt));
  if (ec) {
    detail::rollback_transaction(db.connection);
    return ec;
  }

  sqlite3_int64 note_id = 0;
  ec = detail::lookup_note_id_by_rel_path(db.connection, rel_path, note_id);
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
