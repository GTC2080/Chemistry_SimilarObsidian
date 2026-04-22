// Reason: This file owns persisted PDF metadata state transitions so Track 3
// PDF extraction can evolve without inflating generic attachment state code.

#include "storage/storage_internal.h"

namespace kernel::storage {

std::error_code upsert_pdf_metadata(Database& db, const PdfMetadataRecord& record) {
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  sqlite3_stmt* stmt = nullptr;
  std::error_code ec = detail::prepare(
      db.connection,
      "INSERT INTO pdf_metadata("
      "  rel_path, attachment_content_revision, pdf_metadata_revision, doc_title, "
      "  page_count, has_outline, metadata_state, doc_title_state, text_layer_state"
      ") VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9) "
      "ON CONFLICT(rel_path) DO UPDATE SET "
      "  attachment_content_revision=excluded.attachment_content_revision,"
      "  pdf_metadata_revision=excluded.pdf_metadata_revision,"
      "  doc_title=excluded.doc_title,"
      "  page_count=excluded.page_count,"
      "  has_outline=excluded.has_outline,"
      "  metadata_state=excluded.metadata_state,"
      "  doc_title_state=excluded.doc_title_state,"
      "  text_layer_state=excluded.text_layer_state;",
      &stmt);
  if (ec) {
    return ec;
  }

  sqlite3_bind_text(stmt, 1, record.rel_path.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(
      stmt,
      2,
      record.attachment_content_revision.c_str(),
      -1,
      SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, record.pdf_metadata_revision.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, record.doc_title.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 5, static_cast<sqlite3_int64>(record.page_count));
  sqlite3_bind_int(stmt, 6, record.has_outline ? 1 : 0);
  sqlite3_bind_int(stmt, 7, static_cast<int>(record.metadata_state));
  sqlite3_bind_int(stmt, 8, static_cast<int>(record.doc_title_state));
  sqlite3_bind_int(stmt, 9, static_cast<int>(record.text_layer_state));
  return detail::finalize_with_result(stmt, sqlite3_step(stmt));
}

}  // namespace kernel::storage
