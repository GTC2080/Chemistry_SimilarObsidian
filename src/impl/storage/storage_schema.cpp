// Reason: This file owns storage schema creation and schema-version bookkeeping.

#include "storage/storage_internal.h"

namespace kernel::storage {

std::error_code ensure_schema_v1(Database& db) {
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  std::error_code ec = detail::exec(
      db.connection,
      "PRAGMA journal_mode=WAL;"
      "CREATE TABLE IF NOT EXISTS schema_meta("
      "  key TEXT PRIMARY KEY,"
      "  value TEXT NOT NULL"
      ");"
      "CREATE TABLE IF NOT EXISTS notes("
      "  note_id INTEGER PRIMARY KEY,"
      "  rel_path TEXT NOT NULL UNIQUE,"
      "  title TEXT NOT NULL,"
      "  file_size INTEGER NOT NULL,"
      "  mtime_ns INTEGER NOT NULL,"
      "  content_revision TEXT NOT NULL,"
      "  is_deleted INTEGER NOT NULL DEFAULT 0"
      ");"
      "CREATE TABLE IF NOT EXISTS journal_state("
      "  record_id INTEGER PRIMARY KEY,"
      "  op_id TEXT NOT NULL,"
      "  op_type TEXT NOT NULL,"
      "  rel_path TEXT NOT NULL,"
      "  temp_path TEXT,"
      "  phase TEXT NOT NULL,"
      "  recorded_at_ns INTEGER NOT NULL"
      ");"
      "CREATE TABLE IF NOT EXISTS note_tags("
      "  note_id INTEGER NOT NULL,"
      "  tag TEXT NOT NULL,"
      "  PRIMARY KEY(note_id, tag)"
      ");"
      "CREATE TABLE IF NOT EXISTS note_links("
      "  note_id INTEGER NOT NULL,"
      "  target TEXT NOT NULL,"
      "  PRIMARY KEY(note_id, target)"
      ");"
      "CREATE TABLE IF NOT EXISTS attachments("
      "  attachment_id INTEGER PRIMARY KEY,"
      "  rel_path TEXT NOT NULL UNIQUE,"
      "  file_size INTEGER NOT NULL,"
      "  mtime_ns INTEGER NOT NULL,"
      "  is_missing INTEGER NOT NULL DEFAULT 0"
      ");"
      "CREATE TABLE IF NOT EXISTS pdf_metadata("
      "  rel_path TEXT PRIMARY KEY,"
      "  attachment_content_revision TEXT NOT NULL,"
      "  pdf_metadata_revision TEXT NOT NULL,"
      "  doc_title TEXT NOT NULL,"
      "  page_count INTEGER NOT NULL,"
      "  has_outline INTEGER NOT NULL,"
      "  metadata_state INTEGER NOT NULL,"
      "  doc_title_state INTEGER NOT NULL,"
      "  text_layer_state INTEGER NOT NULL"
      ");"
      "CREATE TABLE IF NOT EXISTS pdf_anchors("
      "  rel_path TEXT NOT NULL,"
      "  page INTEGER NOT NULL,"
      "  pdf_anchor_basis_revision TEXT NOT NULL,"
      "  anchor_serialized TEXT NOT NULL,"
      "  excerpt_text TEXT NOT NULL,"
      "  excerpt_fingerprint TEXT NOT NULL,"
      "  PRIMARY KEY(rel_path, page)"
      ");"
      "CREATE TABLE IF NOT EXISTS note_attachment_refs("
      "  note_id INTEGER NOT NULL,"
      "  attachment_rel_path TEXT NOT NULL,"
      "  PRIMARY KEY(note_id, attachment_rel_path)"
      ");"
      "CREATE TABLE IF NOT EXISTS note_pdf_source_refs("
      "  note_id INTEGER NOT NULL,"
      "  ordinal INTEGER NOT NULL,"
      "  pdf_rel_path TEXT NOT NULL,"
      "  anchor_serialized TEXT NOT NULL,"
      "  page INTEGER NOT NULL,"
      "  excerpt_text TEXT NOT NULL,"
      "  PRIMARY KEY(note_id, ordinal)"
      ");"
      "CREATE TABLE IF NOT EXISTS note_chem_spectrum_refs("
      "  note_id INTEGER NOT NULL,"
      "  ordinal INTEGER NOT NULL,"
      "  attachment_rel_path TEXT NOT NULL,"
      "  selector_serialized TEXT NOT NULL,"
      "  preview_text TEXT NOT NULL,"
      "  PRIMARY KEY(note_id, ordinal)"
      ");"
      "CREATE VIRTUAL TABLE IF NOT EXISTS note_fts USING fts5(title, body, tokenize='unicode61');"
      "CREATE INDEX IF NOT EXISTS idx_notes_is_deleted_rel_path ON notes(is_deleted, rel_path);"
      "CREATE INDEX IF NOT EXISTS idx_note_tags_tag ON note_tags(tag);"
      "CREATE INDEX IF NOT EXISTS idx_note_links_target ON note_links(target);"
      "CREATE INDEX IF NOT EXISTS idx_attachments_is_missing_rel_path ON attachments(is_missing, rel_path);"
      "CREATE INDEX IF NOT EXISTS idx_pdf_metadata_state_rel_path ON pdf_metadata(metadata_state, rel_path);"
      "CREATE INDEX IF NOT EXISTS idx_pdf_anchors_rel_path_page ON pdf_anchors(rel_path, page);"
      "CREATE INDEX IF NOT EXISTS idx_note_pdf_source_refs_pdf_rel_path_note_ordinal "
      "ON note_pdf_source_refs(pdf_rel_path, note_id, ordinal);"
      "CREATE INDEX IF NOT EXISTS idx_note_chem_spectrum_refs_attachment_note_ordinal "
      "ON note_chem_spectrum_refs(attachment_rel_path, note_id, ordinal);");
  if (ec) {
    return ec;
  }

  sqlite3_stmt* stmt = nullptr;
  ec = detail::prepare(
      db.connection,
      "INSERT OR REPLACE INTO schema_meta(key, value) VALUES('schema_version', '8');",
      &stmt);
  if (ec) {
    return ec;
  }
  ec = detail::finalize_with_result(stmt, sqlite3_step(stmt));
  if (ec) {
    return ec;
  }

  return detail::exec(db.connection, "PRAGMA user_version=8;");
}

}  // namespace kernel::storage
