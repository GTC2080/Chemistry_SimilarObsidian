// Reason: This file owns PDF metadata lookup and diagnostics counts so Track 3
// PDF queries stay isolated from the generic attachment query surface.

#include "storage/storage_internal.h"

namespace kernel::storage {

namespace {

constexpr char kLivePdfRowsSql[] =
    "SELECT refs.attachment_rel_path, "
    "       COALESCE(attachments.is_missing, 1), "
    "       meta.attachment_content_revision, "
    "       meta.pdf_metadata_revision, "
    "       meta.doc_title, "
    "       meta.page_count, "
    "       meta.has_outline, "
    "       meta.metadata_state, "
    "       meta.doc_title_state, "
    "       meta.text_layer_state "
    "FROM note_attachment_refs AS refs "
    "JOIN notes ON notes.note_id = refs.note_id AND notes.is_deleted = 0 "
    "LEFT JOIN attachments ON attachments.rel_path = refs.attachment_rel_path "
    "LEFT JOIN pdf_metadata AS meta ON meta.rel_path = refs.attachment_rel_path "
    "WHERE LOWER(refs.attachment_rel_path) LIKE '%.pdf' ";

int state_or_default(sqlite3_stmt* stmt, const int column_index, const int fallback) {
  return sqlite3_column_type(stmt, column_index) == SQLITE_NULL
             ? fallback
             : sqlite3_column_int(stmt, column_index);
}

PdfMetadataRecord read_pdf_metadata_record(sqlite3_stmt* stmt) {
  PdfMetadataRecord record{};
  detail::assign_text_column(stmt, 0, record.rel_path);
  record.is_missing = sqlite3_column_int(stmt, 1) != 0;

  const bool has_metadata_row = sqlite3_column_type(stmt, 2) != SQLITE_NULL;
  if (!has_metadata_row) {
    return record;
  }

  detail::assign_text_column(stmt, 2, record.attachment_content_revision);
  detail::assign_text_column(stmt, 3, record.pdf_metadata_revision);
  detail::assign_text_column(stmt, 4, record.doc_title);
  record.page_count = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 5));
  record.has_outline = sqlite3_column_int(stmt, 6) != 0;
  record.metadata_state = static_cast<PdfMetadataState>(
      state_or_default(stmt, 7, static_cast<int>(PdfMetadataState::Unavailable)));
  record.doc_title_state = static_cast<PdfDocTitleState>(
      state_or_default(stmt, 8, static_cast<int>(PdfDocTitleState::Unavailable)));
  record.text_layer_state = static_cast<PdfTextLayerState>(
      state_or_default(stmt, 9, static_cast<int>(PdfTextLayerState::Unavailable)));
  return record;
}

std::error_code count_live_pdf_rows_with_where_clause(
    sqlite3* db,
    const char* predicate_sql,
    std::uint64_t& out_count) {
  sqlite3_stmt* stmt = nullptr;
  const std::string sql =
      "SELECT COUNT(*) FROM ("
      "  SELECT refs.attachment_rel_path, COALESCE(meta.metadata_state, 0) AS metadata_state "
      "  FROM note_attachment_refs AS refs "
      "  JOIN notes ON notes.note_id = refs.note_id AND notes.is_deleted = 0 "
      "  LEFT JOIN pdf_metadata AS meta ON meta.rel_path = refs.attachment_rel_path "
      "  WHERE LOWER(refs.attachment_rel_path) LIKE '%.pdf' "
      "  GROUP BY refs.attachment_rel_path"
      ") AS live_pdf " +
      std::string(predicate_sql) + ";";
  std::error_code ec = detail::prepare(db, sql.c_str(), &stmt);
  if (ec) {
    return ec;
  }

  const int step_rc = sqlite3_step(stmt);
  if (step_rc != SQLITE_ROW) {
    sqlite3_finalize(stmt);
    return std::error_code(step_rc, std::generic_category());
  }

  out_count = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 0));
  return detail::finalize_with_result(stmt, step_rc);
}

}  // namespace

std::error_code read_live_pdf_metadata_record(
    Database& db,
    std::string_view rel_path,
    PdfMetadataRecord& out_record) {
  out_record = PdfMetadataRecord{};
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  sqlite3_stmt* stmt = nullptr;
  const std::string sql = std::string(kLivePdfRowsSql) +
                          "AND refs.attachment_rel_path = ?1 "
                          "GROUP BY refs.attachment_rel_path;";
  std::error_code ec = detail::prepare(db.connection, sql.c_str(), &stmt);
  if (ec) {
    return ec;
  }

  sqlite3_bind_text(stmt, 1, std::string(rel_path).c_str(), -1, SQLITE_TRANSIENT);
  const int step_rc = sqlite3_step(stmt);
  if (step_rc == SQLITE_DONE) {
    sqlite3_finalize(stmt);
    return std::make_error_code(std::errc::no_such_file_or_directory);
  }
  if (step_rc != SQLITE_ROW) {
    sqlite3_finalize(stmt);
    return std::error_code(step_rc, std::generic_category());
  }

  out_record = read_pdf_metadata_record(stmt);
  return detail::finalize_with_result(stmt, step_rc);
}

std::error_code count_live_pdf_records(Database& db, std::uint64_t& out_count) {
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  return count_live_pdf_rows_with_where_clause(db.connection, "", out_count);
}

std::error_code count_live_pdf_records_by_state(
    Database& db,
    const PdfMetadataState state,
    std::uint64_t& out_count) {
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  const std::string predicate =
      "WHERE metadata_state=" + std::to_string(static_cast<int>(state));
  return count_live_pdf_rows_with_where_clause(
      db.connection,
      predicate.c_str(),
      out_count);
}

}  // namespace kernel::storage
