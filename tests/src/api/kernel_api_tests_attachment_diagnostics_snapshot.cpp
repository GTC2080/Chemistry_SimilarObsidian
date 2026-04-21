// Reason: Keep the large attachment diagnostics export snapshot contract separate from recount lifecycle checks.

#include "kernel/c_api.h"

#include "api/kernel_api_attachment_diagnostics_helpers.h"
#include "api/kernel_api_attachment_diagnostics_suites.h"
#include "api/kernel_api_test_support.h"
#include "core/kernel_internal.h"
#include "support/test_support.h"

#include <filesystem>
#include <string>

namespace {

void test_export_diagnostics_writes_json_snapshot() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);
  const auto export_path = make_attachment_temp_export_path("diagnostics.json");
  std::filesystem::create_directories(vault / "assets");
  write_file_bytes(vault / "assets" / "diag-present.png", "diag-present-bytes");
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "diagnostics snapshot should export from a settled ready state");

  const std::string content =
      "# Diagnostics Title\n"
      "diagnostics-token\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "diag.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));
  const std::string attachment_content =
      "# Diagnostics Attachments\n"
      "![Present](assets/diag-present.png)\n"
      "![Missing](assets/diag-missing.png)\n";
  expect_ok(kernel_write_note(
      handle,
      "diag-attachments.md",
      attachment_content.data(),
      attachment_content.size(),
      nullptr,
      &metadata,
      &disposition));
  {
    std::lock_guard lock(handle->storage_mutex);
    exec_sql(
        handle->storage.connection,
        "INSERT INTO attachments(rel_path, file_size, mtime_ns, is_missing) "
        "VALUES('assets/diag-orphan.bin', 17, 23, 0);");
  }

  expect_ok(kernel_export_diagnostics(handle, export_path.string().c_str()));

  const AttachmentAnomalySnapshot anomaly_snapshot =
      read_attachment_anomaly_snapshot(db_path);
  require_true(
      anomaly_snapshot.missing_attachment_count == 1,
      "diagnostics snapshot should seed exactly one missing live attachment");
  require_true(
      anomaly_snapshot.orphaned_attachment_count >= 1,
      "diagnostics orphan coverage should seed at least one orphaned attachment row");

  const std::string exported = read_file_text(export_path);
  require_true(exported.find("\"session_state\":\"OPEN\"") != std::string::npos, "diagnostics should include session state");
  require_true(exported.find("\"index_state\":\"READY\"") != std::string::npos, "diagnostics should include index state");
  require_true(exported.find("\"index_fault_reason\":\"\"") != std::string::npos, "healthy diagnostics should clear index fault reason");
  require_true(exported.find("\"index_fault_code\":0") != std::string::npos, "healthy diagnostics should clear index fault code");
  require_true(exported.find("\"index_fault_at_ns\":0") != std::string::npos, "healthy diagnostics should clear index fault timestamp");
  require_true(
      exported.find("\"last_recovery_outcome\":\"clean_startup\"") != std::string::npos,
      "healthy diagnostics should report clean_startup as the last recovery outcome");
  require_true(
      exported.find("\"last_recovery_detected_corrupt_tail\":false") != std::string::npos,
      "healthy diagnostics should report no corrupt recovery tail on a clean startup");
  require_true(
      exported.find("\"last_recovery_at_ns\":") != std::string::npos,
      "healthy diagnostics should export last_recovery_at_ns");
  require_true(
      exported.find("\"last_recovery_at_ns\":0") == std::string::npos,
      "healthy diagnostics should export a non-zero last_recovery_at_ns after startup");
  require_true(exported.find("\"indexed_note_count\":2") != std::string::npos, "diagnostics should include indexed note count");
  require_true(exported.find("\"attachment_count\":2") != std::string::npos, "diagnostics should include attachment count");
  require_true(exported.find("\"attachment_live_count\":2") != std::string::npos, "diagnostics should include live attachment count");
  require_true(
      exported.find("\"missing_attachment_count\":" + std::to_string(anomaly_snapshot.missing_attachment_count)) !=
          std::string::npos,
      "diagnostics should include missing attachment count that matches SQLite truth");
  require_true(
      exported.find("\"orphaned_attachment_count\":" + std::to_string(anomaly_snapshot.orphaned_attachment_count)) !=
          std::string::npos,
      "diagnostics should include orphaned attachment count that matches SQLite truth");
  require_true(
      exported.find("\"missing_attachment_paths\":[\"assets/diag-missing.png\"]") != std::string::npos,
      "diagnostics should export a stable missing attachment path summary");
  require_true(
      exported.find("\"orphaned_attachment_paths\":[\"assets/diag-orphan.bin\"]") != std::string::npos,
      "diagnostics should export a stable orphaned attachment path summary");
  require_true(
      exported.find("\"attachment_anomaly_path_summary_limit\":16") != std::string::npos,
      "diagnostics should export the frozen attachment anomaly path summary limit");
  require_true(
      exported.find("\"attachment_anomaly_summary\":\"" + anomaly_snapshot.summary + "\"") !=
          std::string::npos,
      "diagnostics should summarize attachment anomalies from SQLite truth");
  require_true(
      exported.find("\"last_attachment_recount_reason\":\"\"") != std::string::npos,
      "healthy diagnostics should leave last_attachment_recount_reason empty before any rebuild or watcher full rescan");
  require_true(
      exported.find("\"last_attachment_recount_at_ns\":0") != std::string::npos,
      "healthy diagnostics should leave last_attachment_recount_at_ns zero before any rebuild or watcher full rescan");
  require_true(
      exported.find("\"attachment_public_surface_revision\":\"track2_batch1_public_surface_v1\"") != std::string::npos,
      "diagnostics should expose the current attachment public surface revision");
  require_true(
      exported.find("\"attachment_metadata_contract_revision\":\"track2_batch2_metadata_contract_v1\"") !=
          std::string::npos,
      "diagnostics should expose the current attachment metadata contract revision");
  require_true(
      exported.find("\"attachment_kind_mapping_revision\":\"track2_batch1_extension_mapping_v1\"") != std::string::npos,
      "diagnostics should expose the current attachment kind mapping revision");
  require_true(
      exported.find("\"logger_backend\":\"null_logger\"") != std::string::npos,
      "diagnostics should expose the current logger backend");
  require_true(
      exported.find("\"search_contract_revision\":\"track1_batch4_ranking_v1\"") != std::string::npos,
      "diagnostics should expose the current search contract revision");
  require_true(
      exported.find("\"search_backend\":\"sqlite_fts5\"") != std::string::npos,
      "diagnostics should expose the current search backend");
  require_true(
      exported.find("\"search_snippet_mode\":\"body_single_segment_plaintext_fixed_length\"") != std::string::npos,
      "diagnostics should expose the current snippet mode");
  require_true(
      exported.find("\"search_snippet_max_bytes\":160") != std::string::npos,
      "diagnostics should expose the current snippet length cap");
  require_true(
      exported.find("\"search_pagination_mode\":\"offset_limit_exact_total_v1\"") != std::string::npos,
      "diagnostics should expose the current pagination mode");
  require_true(
      exported.find("\"search_filters_mode\":\"kind_tag_path_prefix_v1\"") != std::string::npos,
      "diagnostics should expose the current filters mode");
  require_true(
      exported.find("\"search_ranking_mode\":\"fts_title_tag_v1\"") != std::string::npos,
      "diagnostics should expose the current ranking mode");
  require_true(
      exported.find("\"search_supported_kinds\":\"note,attachment,all\"") != std::string::npos,
      "diagnostics should expose the frozen supported search kinds");
  require_true(
      exported.find("\"search_supported_filters\":\"kind,tag,path_prefix\"") != std::string::npos,
      "diagnostics should expose the frozen supported search filters");
  require_true(
      exported.find("\"search_ranking_supported_kinds\":\"note,all_note_branch\"") != std::string::npos,
      "diagnostics should expose the supported Ranking v1 kinds");
  require_true(
      exported.find("\"search_ranking_tie_break\":\"rel_path_asc\"") != std::string::npos,
      "diagnostics should expose the frozen Ranking v1 tie-break");
  require_true(
      exported.find("\"search_page_max_limit\":128") != std::string::npos,
      "diagnostics should expose the frozen search page max limit");
  require_true(
      exported.find("\"search_total_hits_supported\":true") != std::string::npos,
      "diagnostics should expose that exact total hit counts are supported");
  require_true(
      exported.find("\"search_include_deleted_supported\":false") != std::string::npos,
      "diagnostics should expose that include_deleted remains disabled");
  require_true(
      exported.find("\"search_attachment_path_only\":true") != std::string::npos,
      "diagnostics should expose that attachment search is path-only");
  require_true(
      exported.find("\"search_title_hit_boost_enabled\":true") != std::string::npos,
      "diagnostics should expose that Ranking v1 title-hit boosting is enabled");
  require_true(
      exported.find("\"search_tag_exact_boost_enabled\":true") != std::string::npos,
      "diagnostics should expose that Ranking v1 tag-exact boosting is enabled");
  require_true(
      exported.find("\"search_tag_exact_boost_single_token_only\":true") != std::string::npos,
      "diagnostics should expose the frozen single-token boundary for tag-exact boosting");
  require_true(
      exported.find("\"search_all_kind_order\":\"notes_then_attachments_rel_path_asc\"") != std::string::npos,
      "diagnostics should expose the frozen kind=all ordering");
  require_true(
      exported.find("\"last_continuity_fallback_reason\":\"\"") != std::string::npos,
      "healthy diagnostics should export an empty continuity fallback reason");
  require_true(
      exported.find("\"last_continuity_fallback_at_ns\":0") != std::string::npos,
      "healthy diagnostics should export a zero continuity fallback timestamp before any fallback occurs");
  require_true(exported.find("\"pending_recovery_ops\":0") != std::string::npos, "diagnostics should include pending recovery count");
  require_true(exported.find("\"vault_root\":") != std::string::npos, "diagnostics should include vault root");
  require_true(exported.find("\"state_dir\":") != std::string::npos, "diagnostics should include state dir");
  require_true(exported.find("\"storage_db_path\":") != std::string::npos, "diagnostics should include storage db path");
  require_true(exported.find("\"recovery_journal_path\":") != std::string::npos, "diagnostics should include recovery journal path");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(export_path.parent_path());
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_attachment_diagnostics_snapshot_tests() {
  test_export_diagnostics_writes_json_snapshot();
}
