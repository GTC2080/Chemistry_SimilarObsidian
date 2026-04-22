// Reason: This file owns PDF note-reference read queries so Track 3 Batch 3
// stays isolated from metadata and anchor storage units.

#include "storage/storage_internal.h"

namespace kernel::storage {

namespace {

sqlite3_int64 normalize_limit(const std::size_t limit) {
  return limit == static_cast<std::size_t>(-1)
             ? static_cast<sqlite3_int64>(-1)
             : static_cast<sqlite3_int64>(limit);
}

NotePdfSourceRefRecord read_note_pdf_source_ref_record(sqlite3_stmt* stmt) {
  NotePdfSourceRefRecord record{};
  detail::assign_text_column(stmt, 0, record.pdf_rel_path);
  detail::assign_text_column(stmt, 1, record.anchor_serialized);
  detail::assign_text_column(stmt, 2, record.excerpt_text);
  record.page = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 3));
  return record;
}

PdfSourceReferrerRecord read_pdf_source_referrer_record(sqlite3_stmt* stmt) {
  PdfSourceReferrerRecord record{};
  detail::assign_text_column(stmt, 0, record.note_rel_path);
  detail::assign_text_column(stmt, 1, record.note_title);
  detail::assign_text_column(stmt, 2, record.anchor_serialized);
  detail::assign_text_column(stmt, 3, record.excerpt_text);
  record.page = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 4));
  return record;
}

}  // namespace

std::error_code list_note_pdf_source_ref_records(
    Database& db,
    std::string_view note_rel_path,
    const std::size_t limit,
    std::vector<NotePdfSourceRefRecord>& out_records) {
  out_records.clear();
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  sqlite3_int64 note_id = 0;
  std::error_code ec =
      detail::lookup_active_note_id_by_rel_path(db.connection, note_rel_path, note_id);
  if (ec) {
    return ec;
  }

  sqlite3_stmt* stmt = nullptr;
  ec = detail::prepare(
      db.connection,
      "SELECT pdf_rel_path, anchor_serialized, excerpt_text, page "
      "FROM note_pdf_source_refs "
      "WHERE note_id=?1 "
      "ORDER BY ordinal ASC "
      "LIMIT ?2;",
      &stmt);
  if (ec) {
    return ec;
  }

  sqlite3_bind_int64(stmt, 1, note_id);
  sqlite3_bind_int64(stmt, 2, normalize_limit(limit));
  return detail::append_statement_rows(stmt, [&](sqlite3_stmt* row) {
    out_records.push_back(read_note_pdf_source_ref_record(row));
  });
}

std::error_code list_pdf_source_referrer_records(
    Database& db,
    std::string_view pdf_rel_path,
    const std::size_t limit,
    std::vector<PdfSourceReferrerRecord>& out_records) {
  out_records.clear();
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  sqlite3_stmt* stmt = nullptr;
  std::error_code ec = detail::prepare(
      db.connection,
      "SELECT notes.rel_path, notes.title, refs.anchor_serialized, refs.excerpt_text, refs.page "
      "FROM note_pdf_source_refs AS refs "
      "JOIN notes ON notes.note_id = refs.note_id AND notes.is_deleted = 0 "
      "WHERE refs.pdf_rel_path = ?1 "
      "ORDER BY notes.rel_path ASC, refs.ordinal ASC "
      "LIMIT ?2;",
      &stmt);
  if (ec) {
    return ec;
  }

  sqlite3_bind_text(stmt, 1, std::string(pdf_rel_path).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 2, normalize_limit(limit));
  return detail::append_statement_rows(stmt, [&](sqlite3_stmt* row) {
    out_records.push_back(read_pdf_source_referrer_record(row));
  });
}

}  // namespace kernel::storage
