// Reason: This file owns PDF anchor read queries so Track 3 page-anchor
// substrate can evolve without inflating generic attachment/PDF metadata units.

#include "storage/storage_internal.h"

namespace kernel::storage {
namespace {

PdfAnchorRecord read_pdf_anchor_record(sqlite3_stmt* stmt) {
  PdfAnchorRecord record{};
  detail::assign_text_column(stmt, 0, record.rel_path);
  record.page = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 1));
  detail::assign_text_column(stmt, 2, record.pdf_anchor_basis_revision);
  detail::assign_text_column(stmt, 3, record.anchor_serialized);
  detail::assign_text_column(stmt, 4, record.excerpt_text);
  detail::assign_text_column(stmt, 5, record.excerpt_fingerprint);
  record.is_missing = sqlite3_column_int(stmt, 6) != 0;
  return record;
}

constexpr char kLivePdfAnchorSelect[] =
    "SELECT anchors.rel_path, "
    "       anchors.page, "
    "       anchors.pdf_anchor_basis_revision, "
    "       anchors.anchor_serialized, "
    "       anchors.excerpt_text, "
    "       anchors.excerpt_fingerprint, "
    "       COALESCE(attachments.is_missing, 1) "
    "FROM pdf_anchors AS anchors "
    "JOIN note_attachment_refs AS refs ON refs.attachment_rel_path = anchors.rel_path "
    "JOIN notes ON notes.note_id = refs.note_id AND notes.is_deleted = 0 "
    "LEFT JOIN attachments ON attachments.rel_path = anchors.rel_path ";

}  // namespace

std::error_code list_live_pdf_anchor_records(
    Database& db,
    std::string_view rel_path,
    std::vector<PdfAnchorRecord>& out_records) {
  out_records.clear();
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  sqlite3_stmt* stmt = nullptr;
  const std::string sql = std::string(kLivePdfAnchorSelect) +
                          "WHERE anchors.rel_path = ?1 "
                          "GROUP BY anchors.rel_path, anchors.page "
                          "ORDER BY anchors.page ASC;";
  std::error_code ec = detail::prepare(db.connection, sql.c_str(), &stmt);
  if (ec) {
    return ec;
  }

  sqlite3_bind_text(stmt, 1, std::string(rel_path).c_str(), -1, SQLITE_TRANSIENT);
  bool saw_row = false;
  const std::error_code append_ec = detail::append_statement_rows(stmt, [&](sqlite3_stmt* row) {
    saw_row = true;
    out_records.push_back(read_pdf_anchor_record(row));
  });
  if (!append_ec && !saw_row) {
    return std::make_error_code(std::errc::no_such_file_or_directory);
  }
  return append_ec;
}

std::error_code read_live_pdf_anchor_record(
    Database& db,
    std::string_view rel_path,
    const std::uint64_t page,
    PdfAnchorRecord& out_record) {
  out_record = PdfAnchorRecord{};
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  sqlite3_stmt* stmt = nullptr;
  const std::string sql = std::string(kLivePdfAnchorSelect) +
                          "WHERE anchors.rel_path = ?1 AND anchors.page = ?2 "
                          "GROUP BY anchors.rel_path, anchors.page;";
  std::error_code ec = detail::prepare(db.connection, sql.c_str(), &stmt);
  if (ec) {
    return ec;
  }

  sqlite3_bind_text(stmt, 1, std::string(rel_path).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(page));
  const int step_rc = sqlite3_step(stmt);
  if (step_rc == SQLITE_DONE) {
    sqlite3_finalize(stmt);
    return std::make_error_code(std::errc::no_such_file_or_directory);
  }
  if (step_rc != SQLITE_ROW) {
    sqlite3_finalize(stmt);
    return std::error_code(step_rc, std::generic_category());
  }

  out_record = read_pdf_anchor_record(stmt);
  return detail::finalize_with_result(stmt, step_rc);
}

}  // namespace kernel::storage
