// Reason: Keep attachment recount and last-rebuild diagnostics separate from the broader export snapshot contract.

#include "kernel/c_api.h"

#include "api/kernel_api_attachment_diagnostics_helpers.h"
#include "api/kernel_api_attachment_diagnostics_suites.h"
#include "api/kernel_api_test_support.h"
#include "core/kernel_internal.h"
#include "support/test_support.h"
#include "watcher/session.h"

#include <filesystem>
#include <string>

namespace {

void test_export_diagnostics_reports_last_rebuild_result_and_timestamp() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);
  const auto export_path = make_attachment_temp_export_path("diagnostics-last-rebuild.json");
  std::filesystem::create_directories(vault / "assets");
  write_file_bytes(vault / "assets" / "rebuild-diag.png", "rebuild-diag-bytes");
  const std::string content =
      "# Rebuild Diagnostics Attachment\n"
      "![Figure](assets/rebuild-diag.png)\n";
  write_file_bytes(vault / "rebuild-diagnostics-attachment.md", content);
  kernel_handle* handle = nullptr;
  require_true(
      kernel_open_vault(vault.string().c_str(), &handle).code == KERNEL_OK,
      "last rebuild diagnostics test should open vault");
  require_index_ready(handle, "last rebuild diagnostics test should start from a ready state");

  require_true(
      kernel_rebuild_index(handle).code == KERNEL_OK,
      "last rebuild diagnostics test should complete a successful rebuild");

  require_true(
      kernel_export_diagnostics(handle, export_path.string().c_str()).code == KERNEL_OK,
      "last rebuild diagnostics test should export diagnostics");
  const AttachmentAnomalySnapshot anomaly_snapshot =
      read_attachment_anomaly_snapshot(db_path);
  require_true(
      anomaly_snapshot.missing_attachment_count == 0,
      "successful rebuild diagnostics should leave no missing live attachments");
  require_true(
      anomaly_snapshot.orphaned_attachment_count == 0,
      "successful rebuild diagnostics should leave no orphaned attachment rows");
  const std::string exported = read_file_text(export_path);
  require_true(
      exported.find("\"last_rebuild_result\":\"succeeded\"") != std::string::npos,
      "diagnostics should export the latest successful rebuild result");
  require_true(
      exported.find("\"has_last_rebuild_result\":true") != std::string::npos,
      "diagnostics should report has_last_rebuild_result=true after a successful rebuild");
  require_true(
      exported.find("\"last_rebuild_result_code\":0") != std::string::npos,
      "diagnostics should export the latest successful rebuild result code");
  require_true(
      exported.find("\"last_rebuild_at_ns\":0") == std::string::npos,
      "diagnostics should export a non-zero rebuild timestamp after success");
  require_true(
      exported.find("\"last_rebuild_duration_ms\":") != std::string::npos,
      "diagnostics should export last_rebuild_duration_ms after success");
  require_true(
      exported.find("\"rebuild_current_generation\":0") != std::string::npos,
      "diagnostics should clear the current rebuild generation after rebuild completion");
  require_true(
      exported.find("\"rebuild_last_completed_generation\":1") != std::string::npos,
      "diagnostics should export the latest completed rebuild generation after one successful rebuild");
  require_true(
      exported.find("\"rebuild_current_started_at_ns\":0") != std::string::npos,
      "diagnostics should clear rebuild_current_started_at_ns once no rebuild is in flight");
  require_true(
      exported.find("\"missing_attachment_count\":" + std::to_string(anomaly_snapshot.missing_attachment_count)) !=
          std::string::npos,
      "successful rebuild diagnostics should export missing attachment count that matches SQLite truth");
  require_true(
      exported.find("\"orphaned_attachment_count\":" + std::to_string(anomaly_snapshot.orphaned_attachment_count)) !=
          std::string::npos,
      "successful rebuild diagnostics should export orphaned attachment count that matches SQLite truth");
  require_true(
      exported.find("\"missing_attachment_paths\":[]") != std::string::npos,
      "successful rebuild diagnostics should export an empty missing attachment path summary");
  require_true(
      exported.find("\"orphaned_attachment_paths\":[]") != std::string::npos,
      "successful rebuild diagnostics should export an empty orphaned attachment path summary");
  require_true(
      exported.find("\"attachment_anomaly_summary\":\"" + anomaly_snapshot.summary + "\"") !=
          std::string::npos,
      "successful rebuild diagnostics should summarize attachment anomalies from the exported counts");
  require_true(
      exported.find("\"last_attachment_recount_reason\":\"rebuild\"") != std::string::npos,
      "successful rebuild diagnostics should report attachment recount reason as rebuild");
  require_true(
      exported.find("\"last_attachment_recount_at_ns\":0") == std::string::npos,
      "successful rebuild diagnostics should export a non-zero attachment recount timestamp");

  require_true(
      kernel_close(handle).code == KERNEL_OK,
      "last rebuild diagnostics test should close vault");
  std::filesystem::remove_all(export_path.parent_path());
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_export_diagnostics_reports_last_attachment_recount_after_watcher_full_rescan() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);
  const auto export_path = make_attachment_temp_export_path("diagnostics-watcher-full-rescan.json");
  std::filesystem::create_directories(vault / "assets");
  std::filesystem::create_directories(vault / "docs");
  write_file_bytes(vault / "assets" / "overflow-live.png", "overflow-live-bytes");
  write_file_bytes(vault / "assets" / "overflow-stale.png", "overflow-stale-bytes");
  const std::string before =
      "# Watcher Full Rescan Attachment\n"
      "![Live](assets/overflow-live.png)\n"
      "![Stale](assets/overflow-stale.png)\n";
  write_file_bytes(vault / "watcher-full-rescan-attachment.md", before);

  kernel_handle* handle = nullptr;
  require_true(
      kernel_open_vault(vault.string().c_str(), &handle).code == KERNEL_OK,
      "watcher full-rescan diagnostics test should open vault");
  require_index_ready(handle, "watcher full-rescan diagnostics test should start from a ready state");

  std::filesystem::remove(vault / "assets" / "overflow-stale.png");
  write_file_bytes(vault / "docs" / "overflow-fresh.pdf", "overflow-fresh-bytes");
  const std::string after =
      "# Watcher Full Rescan Attachment\n"
      "![Live](assets/overflow-live.png)\n"
      "![Stale](assets/overflow-stale.png)\n"
      "[Fresh](docs/overflow-fresh.pdf)\n";
  write_file_bytes(vault / "watcher-full-rescan-attachment.md", after);

  kernel::watcher::inject_next_poll_overflow(handle->watcher_session);
  require_eventually(
      [&]() {
        kernel_attachment_list refs{};
        const kernel_status refs_status = kernel_query_note_attachment_refs(
            handle,
            "watcher-full-rescan-attachment.md",
            static_cast<size_t>(-1),
            &refs);
        if (refs_status.code != KERNEL_OK) {
          return false;
        }

        const bool refs_match =
            refs.count == 3 &&
            std::string(refs.attachments[0].rel_path) == "assets/overflow-live.png" &&
            refs.attachments[0].presence == KERNEL_ATTACHMENT_PRESENCE_PRESENT &&
            std::string(refs.attachments[1].rel_path) == "assets/overflow-stale.png" &&
            refs.attachments[1].presence == KERNEL_ATTACHMENT_PRESENCE_MISSING &&
            std::string(refs.attachments[2].rel_path) == "docs/overflow-fresh.pdf" &&
            refs.attachments[2].presence == KERNEL_ATTACHMENT_PRESENCE_PRESENT;
        kernel_free_attachment_list(&refs);

        std::lock_guard runtime_lock(handle->runtime_mutex);
        return refs_match &&
               handle->runtime.last_attachment_recount.reason == "watcher_full_rescan" &&
               handle->runtime.last_attachment_recount.at_ns != 0;
      },
      "watcher full-rescan diagnostics test should observe attachment recount after overflow-driven rescan");

  require_true(
      kernel_export_diagnostics(handle, export_path.string().c_str()).code == KERNEL_OK,
      "watcher full-rescan diagnostics test should export diagnostics");
  const AttachmentAnomalySnapshot anomaly_snapshot =
      read_attachment_anomaly_snapshot(db_path);
  require_true(
      anomaly_snapshot.missing_attachment_count == 1,
      "watcher full-rescan diagnostics should keep exactly one missing live attachment in the seeded scenario");
  require_true(
      anomaly_snapshot.orphaned_attachment_count == 0,
      "watcher full-rescan diagnostics should avoid orphaned attachment rows when exporting outside the watched vault");
  const std::string exported = read_file_text(export_path);
  require_true(
      exported.find("\"last_attachment_recount_reason\":\"watcher_full_rescan\"") != std::string::npos,
      "watcher full-rescan diagnostics should report watcher_full_rescan as the last attachment recount reason");
  require_true(
      exported.find("\"last_attachment_recount_at_ns\":0") == std::string::npos,
      "watcher full-rescan diagnostics should export a non-zero attachment recount timestamp");
  require_true(
      exported.find("\"attachment_anomaly_summary\":\"" + anomaly_snapshot.summary + "\"") !=
          std::string::npos,
      "watcher full-rescan diagnostics should summarize attachment anomalies from SQLite truth");
  require_true(
      exported.find("\"missing_attachment_count\":" + std::to_string(anomaly_snapshot.missing_attachment_count)) !=
          std::string::npos,
      "watcher full-rescan diagnostics should still export refreshed missing attachment count");
  require_true(
      exported.find("\"orphaned_attachment_count\":" + std::to_string(anomaly_snapshot.orphaned_attachment_count)) !=
          std::string::npos,
      "watcher full-rescan diagnostics should report the orphaned attachment count from SQLite truth");
  require_true(
      exported.find("\"missing_attachment_paths\":[\"assets/overflow-stale.png\"]") !=
          std::string::npos,
      "watcher full-rescan diagnostics should export the refreshed missing attachment path summary");
  require_true(
      exported.find("\"orphaned_attachment_paths\":[]") != std::string::npos,
      "watcher full-rescan diagnostics should export an empty orphaned attachment path summary");

  require_true(
      kernel_close(handle).code == KERNEL_OK,
      "watcher full-rescan diagnostics test should close vault");
  std::filesystem::remove_all(export_path.parent_path());
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_attachment_diagnostics_recount_tests() {
  test_export_diagnostics_reports_last_rebuild_result_and_timestamp();
  test_export_diagnostics_reports_last_attachment_recount_after_watcher_full_rescan();
}
