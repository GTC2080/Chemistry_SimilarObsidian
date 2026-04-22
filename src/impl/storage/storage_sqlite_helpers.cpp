// Reason: This file owns the low-level SQLite helpers shared by the split storage implementation units.

#include "storage/storage_internal.h"

#include <chrono>

namespace kernel::storage::detail {

std::error_code sqlite_error(sqlite3*, const int code) {
  if (code == SQLITE_OK || code == SQLITE_DONE || code == SQLITE_ROW) {
    return {};
  }
  return std::error_code(code, std::generic_category());
}

std::error_code exec(sqlite3* db, const char* sql) {
  char* error_message = nullptr;
  const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &error_message);
  if (error_message != nullptr) {
    sqlite3_free(error_message);
  }
  return sqlite_error(db, rc);
}

std::error_code prepare(sqlite3* db, const char* sql, sqlite3_stmt** out_stmt) {
  const int rc = sqlite3_prepare_v2(db, sql, -1, out_stmt, nullptr);
  return sqlite_error(db, rc);
}

std::error_code finalize_with_result(sqlite3_stmt* stmt, const int step_rc) {
  sqlite3_finalize(stmt);
  if (step_rc == SQLITE_DONE || step_rc == SQLITE_ROW) {
    return {};
  }
  return std::error_code(step_rc, std::generic_category());
}

void assign_text_column(sqlite3_stmt* stmt, const int column_index, std::string& out_value) {
  out_value.clear();
  const unsigned char* text = sqlite3_column_text(stmt, column_index);
  if (text != nullptr) {
    out_value = reinterpret_cast<const char*>(text);
  }
}

std::uint64_t now_ns() {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count());
}

std::string title_from_rel_path(std::string_view rel_path) {
  return std::filesystem::path(std::string(rel_path)).stem().string();
}

std::error_code lookup_note_id_by_rel_path(
    sqlite3* db,
    std::string_view rel_path,
    sqlite3_int64& out_note_id) {
  sqlite3_stmt* stmt = nullptr;
  std::error_code ec = prepare(db, "SELECT note_id FROM notes WHERE rel_path=?1;", &stmt);
  if (ec) {
    return ec;
  }

  sqlite3_bind_text(stmt, 1, std::string(rel_path).c_str(), -1, SQLITE_TRANSIENT);
  const int step_rc = sqlite3_step(stmt);
  if (step_rc != SQLITE_ROW) {
    sqlite3_finalize(stmt);
    return std::error_code(step_rc, std::generic_category());
  }

  out_note_id = sqlite3_column_int64(stmt, 0);
  return finalize_with_result(stmt, step_rc);
}

std::error_code lookup_active_note_id_by_rel_path(
    sqlite3* db,
    std::string_view rel_path,
    sqlite3_int64& out_note_id) {
  sqlite3_stmt* stmt = nullptr;
  std::error_code ec = prepare(
      db,
      "SELECT note_id FROM notes WHERE rel_path=?1 AND is_deleted=0;",
      &stmt);
  if (ec) {
    return ec;
  }

  sqlite3_bind_text(stmt, 1, std::string(rel_path).c_str(), -1, SQLITE_TRANSIENT);
  const int step_rc = sqlite3_step(stmt);
  if (step_rc != SQLITE_ROW) {
    sqlite3_finalize(stmt);
    return step_rc == SQLITE_DONE ? std::make_error_code(std::errc::no_such_file_or_directory)
                                  : std::error_code(step_rc, std::generic_category());
  }

  out_note_id = sqlite3_column_int64(stmt, 0);
  return finalize_with_result(stmt, step_rc);
}

std::error_code clear_note_parse_rows(sqlite3* db, const sqlite3_int64 note_id) {
  sqlite3_stmt* stmt = nullptr;
  std::error_code ec = prepare(db, "DELETE FROM note_tags WHERE note_id=?1;", &stmt);
  if (ec) {
    return ec;
  }
  sqlite3_bind_int64(stmt, 1, note_id);
  ec = finalize_with_result(stmt, sqlite3_step(stmt));
  if (ec) {
    return ec;
  }

  ec = prepare(db, "DELETE FROM note_links WHERE note_id=?1;", &stmt);
  if (ec) {
    return ec;
  }
  sqlite3_bind_int64(stmt, 1, note_id);
  return finalize_with_result(stmt, sqlite3_step(stmt));
}

std::error_code clear_note_attachment_rows(sqlite3* db, const sqlite3_int64 note_id) {
  sqlite3_stmt* stmt = nullptr;
  std::error_code ec =
      prepare(db, "DELETE FROM note_attachment_refs WHERE note_id=?1;", &stmt);
  if (ec) {
    return ec;
  }
  sqlite3_bind_int64(stmt, 1, note_id);
  return finalize_with_result(stmt, sqlite3_step(stmt));
}

std::error_code clear_note_pdf_source_ref_rows(sqlite3* db, const sqlite3_int64 note_id) {
  sqlite3_stmt* stmt = nullptr;
  std::error_code ec =
      prepare(db, "DELETE FROM note_pdf_source_refs WHERE note_id=?1;", &stmt);
  if (ec) {
    return ec;
  }
  sqlite3_bind_int64(stmt, 1, note_id);
  return finalize_with_result(stmt, sqlite3_step(stmt));
}

std::error_code clear_note_chem_spectrum_source_ref_rows(
    sqlite3* db,
    const sqlite3_int64 note_id) {
  sqlite3_stmt* stmt = nullptr;
  std::error_code ec =
      prepare(db, "DELETE FROM note_chem_spectrum_refs WHERE note_id=?1;", &stmt);
  if (ec) {
    return ec;
  }
  sqlite3_bind_int64(stmt, 1, note_id);
  return finalize_with_result(stmt, sqlite3_step(stmt));
}

std::error_code mark_note_fts_deleted(sqlite3* db, const sqlite3_int64 note_id) {
  sqlite3_stmt* stmt = nullptr;
  std::error_code ec = prepare(db, "DELETE FROM note_fts WHERE rowid=?1;", &stmt);
  if (ec) {
    return ec;
  }
  sqlite3_bind_int64(stmt, 1, note_id);
  return finalize_with_result(stmt, sqlite3_step(stmt));
}

std::error_code insert_note_tag(sqlite3* db, const sqlite3_int64 note_id, std::string_view tag) {
  sqlite3_stmt* stmt = nullptr;
  std::error_code ec = prepare(db, "INSERT INTO note_tags(note_id, tag) VALUES(?1, ?2);", &stmt);
  if (ec) {
    return ec;
  }
  sqlite3_bind_int64(stmt, 1, note_id);
  sqlite3_bind_text(stmt, 2, std::string(tag).c_str(), -1, SQLITE_TRANSIENT);
  return finalize_with_result(stmt, sqlite3_step(stmt));
}

std::error_code insert_note_link(sqlite3* db, const sqlite3_int64 note_id, std::string_view target) {
  sqlite3_stmt* stmt = nullptr;
  std::error_code ec = prepare(db, "INSERT INTO note_links(note_id, target) VALUES(?1, ?2);", &stmt);
  if (ec) {
    return ec;
  }
  sqlite3_bind_int64(stmt, 1, note_id);
  sqlite3_bind_text(stmt, 2, std::string(target).c_str(), -1, SQLITE_TRANSIENT);
  return finalize_with_result(stmt, sqlite3_step(stmt));
}

std::error_code insert_note_attachment_ref(
    sqlite3* db,
    const sqlite3_int64 note_id,
    std::string_view attachment_rel_path) {
  sqlite3_stmt* stmt = nullptr;
  std::error_code ec = prepare(
      db,
      "INSERT INTO note_attachment_refs(note_id, attachment_rel_path) VALUES(?1, ?2);",
      &stmt);
  if (ec) {
    return ec;
  }
  sqlite3_bind_int64(stmt, 1, note_id);
  sqlite3_bind_text(stmt, 2, std::string(attachment_rel_path).c_str(), -1, SQLITE_TRANSIENT);
  return finalize_with_result(stmt, sqlite3_step(stmt));
}

std::error_code insert_note_pdf_source_ref(
    sqlite3* db,
    const sqlite3_int64 note_id,
    const std::int64_t ordinal,
    std::string_view pdf_rel_path,
    std::string_view anchor_serialized,
    const std::uint64_t page,
    std::string_view excerpt_text) {
  sqlite3_stmt* stmt = nullptr;
  std::error_code ec = prepare(
      db,
      "INSERT INTO note_pdf_source_refs("
      "  note_id, ordinal, pdf_rel_path, anchor_serialized, page, excerpt_text"
      ") VALUES(?1, ?2, ?3, ?4, ?5, ?6);",
      &stmt);
  if (ec) {
    return ec;
  }
  sqlite3_bind_int64(stmt, 1, note_id);
  sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(ordinal));
  sqlite3_bind_text(stmt, 3, std::string(pdf_rel_path).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, std::string(anchor_serialized).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 5, static_cast<sqlite3_int64>(page));
  sqlite3_bind_text(stmt, 6, std::string(excerpt_text).c_str(), -1, SQLITE_TRANSIENT);
  return finalize_with_result(stmt, sqlite3_step(stmt));
}

std::error_code insert_note_chem_spectrum_source_ref(
    sqlite3* db,
    const sqlite3_int64 note_id,
    const std::int64_t ordinal,
    std::string_view attachment_rel_path,
    std::string_view selector_serialized,
    std::string_view preview_text) {
  sqlite3_stmt* stmt = nullptr;
  std::error_code ec = prepare(
      db,
      "INSERT INTO note_chem_spectrum_refs("
      "  note_id, ordinal, attachment_rel_path, selector_serialized, preview_text"
      ") VALUES(?1, ?2, ?3, ?4, ?5);",
      &stmt);
  if (ec) {
    return ec;
  }
  sqlite3_bind_int64(stmt, 1, note_id);
  sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(ordinal));
  sqlite3_bind_text(stmt, 3, std::string(attachment_rel_path).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, std::string(selector_serialized).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 5, std::string(preview_text).c_str(), -1, SQLITE_TRANSIENT);
  return finalize_with_result(stmt, sqlite3_step(stmt));
}

std::error_code replace_note_fts_row(
    sqlite3* db,
    const sqlite3_int64 note_id,
    std::string_view title,
    std::string_view body_text) {
  sqlite3_stmt* stmt = nullptr;
  std::error_code ec = prepare(db, "DELETE FROM note_fts WHERE rowid=?1;", &stmt);
  if (ec) {
    return ec;
  }
  sqlite3_bind_int64(stmt, 1, note_id);
  ec = finalize_with_result(stmt, sqlite3_step(stmt));
  if (ec) {
    return ec;
  }

  ec = prepare(db, "INSERT INTO note_fts(rowid, title, body) VALUES(?1, ?2, ?3);", &stmt);
  if (ec) {
    return ec;
  }
  sqlite3_bind_int64(stmt, 1, note_id);
  sqlite3_bind_text(stmt, 2, std::string(title).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, std::string(body_text).c_str(), -1, SQLITE_TRANSIENT);
  return finalize_with_result(stmt, sqlite3_step(stmt));
}

std::error_code scalar_count_query(sqlite3* db, const char* sql, std::uint64_t& out_count) {
  sqlite3_stmt* stmt = nullptr;
  std::error_code ec = prepare(db, sql, &stmt);
  if (ec) {
    return ec;
  }

  const int step_rc = sqlite3_step(stmt);
  if (step_rc != SQLITE_ROW) {
    sqlite3_finalize(stmt);
    return std::error_code(step_rc, std::generic_category());
  }

  out_count = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 0));
  return finalize_with_result(stmt, step_rc);
}

std::error_code begin_transaction(sqlite3* db) {
  return exec(db, "BEGIN IMMEDIATE;");
}

std::error_code commit_transaction(sqlite3* db) {
  return exec(db, "COMMIT;");
}

void rollback_transaction(sqlite3* db) {
  exec(db, "ROLLBACK;");
}

}  // namespace kernel::storage::detail
