// Reason: This file owns persisted PDF anchor replacement so Track 3 page
// anchors can be rebuilt atomically without bloating generic storage units.

#include "storage/storage_internal.h"

namespace kernel::storage {

std::error_code replace_pdf_anchor_records(
    Database& db,
    std::string_view rel_path,
    const std::vector<PdfAnchorRecord>& records) {
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  std::error_code ec = detail::begin_transaction(db.connection);
  if (ec) {
    return ec;
  }

  sqlite3_stmt* delete_stmt = nullptr;
  ec = detail::prepare(
      db.connection,
      "DELETE FROM pdf_anchors WHERE rel_path=?1;",
      &delete_stmt);
  if (ec) {
    detail::rollback_transaction(db.connection);
    return ec;
  }

  sqlite3_bind_text(delete_stmt, 1, std::string(rel_path).c_str(), -1, SQLITE_TRANSIENT);
  ec = detail::finalize_with_result(delete_stmt, sqlite3_step(delete_stmt));
  if (ec) {
    detail::rollback_transaction(db.connection);
    return ec;
  }

  if (!records.empty()) {
    sqlite3_stmt* insert_stmt = nullptr;
    ec = detail::prepare(
        db.connection,
        "INSERT INTO pdf_anchors("
        "  rel_path, page, pdf_anchor_basis_revision, anchor_serialized, "
        "  excerpt_text, excerpt_fingerprint"
        ") VALUES(?1, ?2, ?3, ?4, ?5, ?6);",
        &insert_stmt);
    if (ec) {
      detail::rollback_transaction(db.connection);
      return ec;
    }

    for (const auto& record : records) {
      sqlite3_reset(insert_stmt);
      sqlite3_clear_bindings(insert_stmt);
      sqlite3_bind_text(insert_stmt, 1, record.rel_path.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_int64(insert_stmt, 2, static_cast<sqlite3_int64>(record.page));
      sqlite3_bind_text(
          insert_stmt,
          3,
          record.pdf_anchor_basis_revision.c_str(),
          -1,
          SQLITE_TRANSIENT);
      sqlite3_bind_text(
          insert_stmt,
          4,
          record.anchor_serialized.c_str(),
          -1,
          SQLITE_TRANSIENT);
      sqlite3_bind_text(
          insert_stmt,
          5,
          record.excerpt_text.c_str(),
          -1,
          SQLITE_TRANSIENT);
      sqlite3_bind_text(
          insert_stmt,
          6,
          record.excerpt_fingerprint.c_str(),
          -1,
          SQLITE_TRANSIENT);
      const int step_rc = sqlite3_step(insert_stmt);
      if (step_rc != SQLITE_DONE) {
        sqlite3_finalize(insert_stmt);
        detail::rollback_transaction(db.connection);
        return std::error_code(step_rc, std::generic_category());
      }
    }

    sqlite3_finalize(insert_stmt);
  }

  ec = detail::commit_transaction(db.connection);
  if (ec) {
    detail::rollback_transaction(db.connection);
  }
  return ec;
}

}  // namespace kernel::storage
