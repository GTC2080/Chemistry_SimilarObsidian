// Reason: This file centralizes private SQLite helper declarations shared by the split storage implementation units.

#pragma once

#include "storage/storage.h"

#include "third_party/sqlite/sqlite3.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>

namespace kernel::storage::detail {

std::error_code sqlite_error(sqlite3* db, int code);
std::error_code exec(sqlite3* db, const char* sql);
std::error_code prepare(sqlite3* db, const char* sql, sqlite3_stmt** out_stmt);
std::error_code finalize_with_result(sqlite3_stmt* stmt, int step_rc);
void assign_text_column(sqlite3_stmt* stmt, int column_index, std::string& out_value);

template <typename AppendRowFn>
std::error_code append_statement_rows(sqlite3_stmt* stmt, AppendRowFn&& append_row) {
  while (true) {
    const int step_rc = sqlite3_step(stmt);
    if (step_rc == SQLITE_DONE) {
      sqlite3_finalize(stmt);
      return {};
    }
    if (step_rc != SQLITE_ROW) {
      sqlite3_finalize(stmt);
      return std::error_code(step_rc, std::generic_category());
    }
    append_row(stmt);
  }
}

std::uint64_t now_ns();
std::string title_from_rel_path(std::string_view rel_path);

std::error_code lookup_note_id_by_rel_path(
    sqlite3* db,
    std::string_view rel_path,
    sqlite3_int64& out_note_id);
std::error_code lookup_active_note_id_by_rel_path(
    sqlite3* db,
    std::string_view rel_path,
    sqlite3_int64& out_note_id);
std::error_code clear_note_parse_rows(sqlite3* db, sqlite3_int64 note_id);
std::error_code clear_note_attachment_rows(sqlite3* db, sqlite3_int64 note_id);
std::error_code clear_note_pdf_source_ref_rows(sqlite3* db, sqlite3_int64 note_id);
std::error_code clear_note_chem_spectrum_source_ref_rows(sqlite3* db, sqlite3_int64 note_id);
std::error_code mark_note_fts_deleted(sqlite3* db, sqlite3_int64 note_id);
std::error_code insert_note_tag(sqlite3* db, sqlite3_int64 note_id, std::string_view tag);
std::error_code insert_note_link(sqlite3* db, sqlite3_int64 note_id, std::string_view target);
std::error_code insert_note_attachment_ref(
    sqlite3* db,
    sqlite3_int64 note_id,
    std::string_view attachment_rel_path);
std::error_code insert_note_pdf_source_ref(
    sqlite3* db,
    sqlite3_int64 note_id,
    std::int64_t ordinal,
    std::string_view pdf_rel_path,
    std::string_view anchor_serialized,
    std::uint64_t page,
    std::string_view excerpt_text);
std::error_code insert_note_chem_spectrum_source_ref(
    sqlite3* db,
    sqlite3_int64 note_id,
    std::int64_t ordinal,
    std::string_view attachment_rel_path,
    std::string_view selector_serialized,
    std::string_view preview_text);
std::error_code replace_note_fts_row(
    sqlite3* db,
    sqlite3_int64 note_id,
    std::string_view title,
    std::string_view body_text);
std::error_code scalar_count_query(sqlite3* db, const char* sql, std::uint64_t& out_count);

std::error_code begin_transaction(sqlite3* db);
std::error_code commit_transaction(sqlite3* db);
void rollback_transaction(sqlite3* db);

}  // namespace kernel::storage::detail
