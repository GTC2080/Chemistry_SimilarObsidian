// Reason: Share attachment diagnostics anomaly helpers across the slimmer attachment diagnostics suites.

#include "api/kernel_api_attachment_diagnostics_helpers.h"

#include "api/kernel_api_test_support.h"
#include "support/test_support.h"

#include "third_party/sqlite/sqlite3.h"

std::string summarize_attachment_anomalies(
    const int missing_attachment_count,
    const int orphaned_attachment_count) {
  if (missing_attachment_count != 0 && orphaned_attachment_count != 0) {
    return "missing_live_and_orphaned_attachments";
  }
  if (missing_attachment_count != 0) {
    return "missing_live_attachments";
  }
  if (orphaned_attachment_count != 0) {
    return "orphaned_attachments";
  }
  return "clean";
}

std::filesystem::path make_attachment_temp_export_path(std::string_view filename) {
  return make_temp_vault("chem_kernel_export_") / std::string(filename);
}

AttachmentAnomalySnapshot read_attachment_anomaly_snapshot(
    const std::filesystem::path& db_path) {
  sqlite3* db = open_sqlite_readonly(db_path);
  AttachmentAnomalySnapshot snapshot{};
  snapshot.missing_attachment_count = query_single_int(
      db,
      "SELECT COUNT(*) "
      "FROM ("
      "  SELECT DISTINCT note_attachment_refs.attachment_rel_path "
      "  FROM note_attachment_refs "
      "  JOIN notes ON notes.note_id = note_attachment_refs.note_id "
      "  JOIN attachments ON attachments.rel_path = note_attachment_refs.attachment_rel_path "
      "  WHERE notes.is_deleted = 0 AND attachments.is_missing = 1"
      ");");
  snapshot.orphaned_attachment_count = query_single_int(
      db,
      "SELECT COUNT(*) "
      "FROM attachments "
      "LEFT JOIN ("
      "  SELECT DISTINCT note_attachment_refs.attachment_rel_path "
      "  FROM note_attachment_refs "
      "  JOIN notes ON notes.note_id = note_attachment_refs.note_id "
      "  WHERE notes.is_deleted = 0"
      ") AS live_refs "
      "  ON live_refs.attachment_rel_path = attachments.rel_path "
      "WHERE live_refs.attachment_rel_path IS NULL;");
  sqlite3_close(db);
  snapshot.summary = summarize_attachment_anomalies(
      snapshot.missing_attachment_count,
      snapshot.orphaned_attachment_count);
  return snapshot;
}
