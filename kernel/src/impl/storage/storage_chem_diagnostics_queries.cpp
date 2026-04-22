// Reason: This file owns chemistry diagnostics-only storage reads so support
// bundle collection stays separate from formal chemistry query surfaces.

#include "storage/storage_internal.h"

namespace kernel::storage {

std::error_code list_live_chem_spectrum_source_ref_diagnostics_records(
    Database& db,
    std::vector<NoteChemSpectrumSourceRefRecord>& out_records) {
  out_records.clear();
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  sqlite3_stmt* stmt = nullptr;
  std::error_code ec = detail::prepare(
      db.connection,
      "SELECT refs.attachment_rel_path, refs.selector_serialized, refs.preview_text "
      "FROM note_chem_spectrum_refs AS refs "
      "JOIN notes ON notes.note_id = refs.note_id AND notes.is_deleted = 0 "
      "ORDER BY notes.rel_path ASC, refs.ordinal ASC;",
      &stmt);
  if (ec) {
    return ec;
  }

  return detail::append_statement_rows(stmt, [&](sqlite3_stmt* row) {
    NoteChemSpectrumSourceRefRecord record{};
    detail::assign_text_column(row, 0, record.attachment_rel_path);
    detail::assign_text_column(row, 1, record.selector_serialized);
    detail::assign_text_column(row, 2, record.preview_text);
    out_records.push_back(std::move(record));
  });
}

}  // namespace kernel::storage
