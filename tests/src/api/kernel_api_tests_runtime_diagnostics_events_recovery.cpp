// Reason: This file isolates runtime diagnostics event and recovery coverage so rebuild and state diagnostics can stay separate.

#include "kernel/c_api.h"

#include "api/kernel_api_runtime_diagnostics_core_suites.h"
#include "api/kernel_api_test_support.h"
#include "core/kernel_shared.h"
#include "index/refresh.h"
#include "recovery/journal.h"
#include "support/test_support.h"

#include <filesystem>
#include <string>

namespace {

void test_export_diagnostics_reports_recent_events_in_runtime_order() {
  const auto vault = make_temp_vault();
  const auto export_path = vault / "diagnostics-recent-events.json";
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "recent-events diagnostics test should start from a ready state");

  kernel::core::record_continuity_fallback(handle, "rename_old_without_new");
  kernel::index::inject_full_rescan_delay_ms(300, 1);
  expect_ok(kernel_rebuild_index(handle));
  kernel::index::inject_full_rescan_delay_ms(0, 0);

  expect_ok(kernel_export_diagnostics(handle, export_path.string().c_str()));
  const std::string exported = read_file_text(export_path);
  require_true(
      exported.find("\"recent_events\":[") != std::string::npos,
      "diagnostics should export a recent_events array");

  const std::size_t recovery_pos = exported.find("\"kind\":\"startup_recovery\"");
  const std::size_t continuity_pos = exported.find("\"kind\":\"continuity_fallback\"");
  const std::size_t rebuild_started_pos = exported.find("\"kind\":\"rebuild_started\"");
  const std::size_t rebuild_succeeded_pos = exported.find("\"kind\":\"rebuild_succeeded\"");
  require_true(
      recovery_pos != std::string::npos &&
          continuity_pos != std::string::npos &&
          rebuild_started_pos != std::string::npos &&
          rebuild_succeeded_pos != std::string::npos,
      "diagnostics recent_events should include startup recovery, continuity fallback, rebuild_started, and rebuild_succeeded");
  require_true(
      recovery_pos < continuity_pos &&
          continuity_pos < rebuild_started_pos &&
          rebuild_started_pos < rebuild_succeeded_pos,
      "diagnostics recent_events should retain stable append order");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_export_diagnostics_reports_last_continuity_fallback_reason() {
  const auto vault = make_temp_vault();
  const auto export_path = vault / "diagnostics-continuity-fallback.json";
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "continuity fallback diagnostics test should start from a ready state");

  kernel::core::record_continuity_fallback(handle, "rename_old_without_new");

  expect_ok(kernel_export_diagnostics(handle, export_path.string().c_str()));
  const std::string exported = read_file_text(export_path);
  require_true(
      exported.find("\"last_continuity_fallback_reason\":\"rename_old_without_new\"") != std::string::npos,
      "diagnostics should export the latest continuity fallback reason");
  require_true(
      exported.find("\"last_continuity_fallback_at_ns\":0") == std::string::npos,
      "diagnostics should export a non-zero continuity fallback timestamp after a fallback is recorded");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_export_diagnostics_reports_recovery_outcome_after_corrupt_tail_cleanup() {
  const auto vault = make_temp_vault();
  const auto state_dir = state_dir_for_vault(vault);
  const auto journal_path = journal_path_for_vault(vault);
  const auto export_path = vault / "diagnostics-recovery-outcome.json";
  const auto target_path = vault / "diag-recover.md";
  const auto temp_path = target_path.parent_path() / "diag-recover.md.codex-recovery.tmp";

  prepare_state_dir_for_vault(vault);
  write_file_bytes(
      target_path,
      "# Recovery Outcome\n"
      "recovery-outcome-token\n");
  write_file_bytes(temp_path, "stale-temp");

  require_true(
      kernel::recovery::append_save_begin(
          journal_path,
          "diag-recovery-op",
          "diag-recover.md",
          temp_path)
              .value() == 0,
      "manual SAVE_BEGIN append should succeed before corrupt-tail diagnostics test");
  append_crc_mismatch_tail_record(journal_path);

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "recovery-outcome diagnostics test should settle to READY after startup recovery");

  expect_ok(kernel_export_diagnostics(handle, export_path.string().c_str()));
  const std::string exported = read_file_text(export_path);
  require_true(
      exported.find("\"last_recovery_outcome\":\"recovered_pending_saves\"") != std::string::npos,
      "diagnostics should report recovered_pending_saves after replaying startup recovery work");
  require_true(
      exported.find("\"last_recovery_detected_corrupt_tail\":true") != std::string::npos,
      "diagnostics should report that startup recovery detected and ignored a corrupt tail");
  require_true(
      exported.find("\"last_recovery_at_ns\":") != std::string::npos,
      "diagnostics should export last_recovery_at_ns after startup recovery work");
  require_true(
      exported.find("\"last_recovery_at_ns\":0") == std::string::npos,
      "diagnostics should export a non-zero last_recovery_at_ns after replaying startup recovery work");
  require_true(
      exported.find("\"pending_recovery_ops\":0") != std::string::npos,
      "diagnostics should still report zero pending recovery ops after recovery cleanup");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir);
}

void test_export_diagnostics_reports_temp_only_recovery_cleanup() {
  const auto vault = make_temp_vault();
  const auto state_dir = state_dir_for_vault(vault);
  const auto journal_path = journal_path_for_vault(vault);
  const auto export_path = vault / "diagnostics-temp-only-recovery.json";
  const auto temp_path = vault / "temp-only-recovery.tmp";

  prepare_state_dir_for_vault(vault);
  write_file_bytes(temp_path, "orphan-temp");

  require_true(
      kernel::recovery::append_save_begin(
          journal_path,
          "temp-only-diag-op",
          "missing.md",
          temp_path)
              .value() == 0,
      "temp-only SAVE_BEGIN append should succeed before diagnostics recovery-outcome test");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "temp-only diagnostics recovery test should settle to READY after cleanup");

  expect_ok(kernel_export_diagnostics(handle, export_path.string().c_str()));
  const std::string exported = read_file_text(export_path);
  require_true(
      exported.find("\"last_recovery_outcome\":\"cleaned_temp_only_pending_saves\"") != std::string::npos,
      "diagnostics should distinguish temp-only recovery cleanup from note recovery");
  require_true(
      exported.find("\"last_recovery_detected_corrupt_tail\":false") != std::string::npos,
      "temp-only recovery cleanup should not imply a corrupt journal tail");
  require_true(
      exported.find("\"last_recovery_at_ns\":") != std::string::npos,
      "diagnostics should export last_recovery_at_ns after temp-only startup cleanup");
  require_true(
      exported.find("\"last_recovery_at_ns\":0") == std::string::npos,
      "diagnostics should export a non-zero last_recovery_at_ns after temp-only startup cleanup");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir);
}

}  // namespace

void run_runtime_diagnostics_event_recovery_tests() {
  test_export_diagnostics_reports_recent_events_in_runtime_order();
  test_export_diagnostics_reports_last_continuity_fallback_reason();
  test_export_diagnostics_reports_recovery_outcome_after_corrupt_tail_cleanup();
  test_export_diagnostics_reports_temp_only_recovery_cleanup();
}
