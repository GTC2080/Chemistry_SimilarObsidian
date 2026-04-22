// Reason: This file owns PDF diagnostics storage summaries so Batch 4 support
// bundle growth stays separate from metadata, anchor, and public query units.

#include "storage/storage_internal.h"

namespace kernel::storage {

namespace {

constexpr char kLivePdfRowsSql[] =
    "FROM note_attachment_refs AS refs "
    "JOIN notes ON notes.note_id = refs.note_id AND notes.is_deleted = 0 "
    "WHERE LOWER(refs.attachment_rel_path) LIKE '%.pdf' ";

}  // namespace

std::error_code count_live_pdf_anchor_records(Database& db, std::uint64_t& out_count) {
  out_count = 0;
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  sqlite3_stmt* stmt = nullptr;
  const std::string sql =
      "SELECT COUNT(*) "
      "FROM ("
      "  SELECT anchors.rel_path, anchors.page "
      "  FROM pdf_anchors AS anchors "
      "  JOIN note_attachment_refs AS refs ON refs.attachment_rel_path = anchors.rel_path "
      "  JOIN notes ON notes.note_id = refs.note_id AND notes.is_deleted = 0 "
      "  GROUP BY anchors.rel_path, anchors.page"
      ") AS live_pdf_anchors;";
  std::error_code ec = detail::prepare(db.connection, sql.c_str(), &stmt);
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

std::error_code list_live_pdf_source_ref_diagnostics_records(
    Database& db,
    std::vector<PdfSourceRefDiagnosticsRecord>& out_records) {
  out_records.clear();
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  sqlite3_stmt* stmt = nullptr;
  const std::string sql =
      "SELECT notes.rel_path, refs.pdf_rel_path, refs.anchor_serialized, refs.excerpt_text, refs.page "
      "FROM note_pdf_source_refs AS refs "
      "JOIN notes ON notes.note_id = refs.note_id AND notes.is_deleted = 0 "
      "ORDER BY notes.rel_path ASC, refs.ordinal ASC;";
  std::error_code ec = detail::prepare(db.connection, sql.c_str(), &stmt);
  if (ec) {
    return ec;
  }

  return detail::append_statement_rows(stmt, [&](sqlite3_stmt* row) {
    PdfSourceRefDiagnosticsRecord record{};
    detail::assign_text_column(row, 0, record.note_rel_path);
    detail::assign_text_column(row, 1, record.pdf_rel_path);
    detail::assign_text_column(row, 2, record.anchor_serialized);
    detail::assign_text_column(row, 3, record.excerpt_text);
    record.page = static_cast<std::uint64_t>(sqlite3_column_int64(row, 4));
    out_records.push_back(std::move(record));
  });
}

}  // namespace kernel::storage
