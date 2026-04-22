// Reason: This file owns chemistry spectrum note-reference read queries so
// Track 5 Batch 3 stays isolated from subtype and metadata storage units.

#include "storage/storage_internal.h"

namespace kernel::storage {
namespace {

sqlite3_int64 normalize_limit(const std::size_t limit) {
  return limit == static_cast<std::size_t>(-1)
             ? static_cast<sqlite3_int64>(-1)
             : static_cast<sqlite3_int64>(limit);
}

NoteChemSpectrumSourceRefRecord read_note_chem_spectrum_source_ref_record(sqlite3_stmt* stmt) {
  NoteChemSpectrumSourceRefRecord record{};
  detail::assign_text_column(stmt, 0, record.attachment_rel_path);
  detail::assign_text_column(stmt, 1, record.selector_serialized);
  detail::assign_text_column(stmt, 2, record.preview_text);
  return record;
}

ChemSpectrumReferrerRecord read_chem_spectrum_referrer_record(sqlite3_stmt* stmt) {
  ChemSpectrumReferrerRecord record{};
  detail::assign_text_column(stmt, 0, record.note_rel_path);
  detail::assign_text_column(stmt, 1, record.note_title);
  detail::assign_text_column(stmt, 2, record.selector_serialized);
  detail::assign_text_column(stmt, 3, record.preview_text);
  return record;
}

}  // namespace

std::error_code list_note_chem_spectrum_source_ref_records(
    Database& db,
    std::string_view note_rel_path,
    const std::size_t limit,
    std::vector<NoteChemSpectrumSourceRefRecord>& out_records) {
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
      "SELECT attachment_rel_path, selector_serialized, preview_text "
      "FROM note_chem_spectrum_refs "
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
    out_records.push_back(read_note_chem_spectrum_source_ref_record(row));
  });
}

std::error_code list_chem_spectrum_referrer_records(
    Database& db,
    std::string_view attachment_rel_path,
    const std::size_t limit,
    std::vector<ChemSpectrumReferrerRecord>& out_records) {
  out_records.clear();
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  sqlite3_stmt* stmt = nullptr;
  std::error_code ec = detail::prepare(
      db.connection,
      "SELECT notes.rel_path, notes.title, refs.selector_serialized, refs.preview_text "
      "FROM note_chem_spectrum_refs AS refs "
      "JOIN notes ON notes.note_id = refs.note_id AND notes.is_deleted = 0 "
      "WHERE refs.attachment_rel_path = ?1 "
      "ORDER BY notes.rel_path ASC, refs.ordinal ASC "
      "LIMIT ?2;",
      &stmt);
  if (ec) {
    return ec;
  }

  sqlite3_bind_text(stmt, 1, std::string(attachment_rel_path).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 2, normalize_limit(limit));
  return detail::append_statement_rows(stmt, [&](sqlite3_stmt* row) {
    out_records.push_back(read_chem_spectrum_referrer_record(row));
  });
}

}  // namespace kernel::storage
