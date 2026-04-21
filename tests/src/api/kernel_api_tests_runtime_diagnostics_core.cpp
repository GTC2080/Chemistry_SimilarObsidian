// Reason: Keep runtime diagnostics coverage for recovery, catch-up, and rebuild separate from watcher fault/backoff behavior.

#include "kernel/c_api.h"

#include "api/kernel_api_runtime_diagnostics_suites.h"
#include "api/kernel_api_test_support.h"
#include "core/kernel_shared.h"
#include "index/refresh.h"
#include "recovery/journal.h"
#include "support/test_support.h"

#include <filesystem>
#include <string>
#include <thread>

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

void test_export_diagnostics_reflects_catching_up_without_fault_and_without_stale_count() {
  const auto vault = make_temp_vault();
  const auto export_path = vault / "diagnostics-catching-up.json";
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string original =
      "# Catching Up Export Title\n"
      "catching-up-export-token\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "catching-up-export.md",
      original.data(),
      original.size(),
      nullptr,
      &metadata,
      &disposition));
  require_index_ready(handle, "seed write should settle before close");
  expect_ok(kernel_close(handle));

  std::filesystem::remove(vault / "catching-up-export.md");
  kernel::index::inject_full_rescan_delay_ms(300, 1);

  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_CATCHING_UP;
      },
      "delayed reopen should expose CATCHING_UP before diagnostics export");

  expect_ok(kernel_export_diagnostics(handle, export_path.string().c_str()));
  const std::string exported = read_file_text(export_path);
  require_true(
      exported.find("\"index_state\":\"CATCHING_UP\"") != std::string::npos,
      "diagnostics should export CATCHING_UP during initial reconciliation");
  require_true(
      exported.find("\"index_fault_reason\":\"\"") != std::string::npos,
      "diagnostics should keep fault reason empty during healthy catch-up");
  require_true(
      exported.find("\"index_fault_code\":0") != std::string::npos,
      "diagnostics should keep fault code zero during healthy catch-up");
  require_true(
      exported.find("\"index_fault_at_ns\":0") != std::string::npos,
      "diagnostics should keep fault timestamp zero during healthy catch-up");
  require_true(
      exported.find("\"indexed_note_count\":0") != std::string::npos,
      "diagnostics should not export a stale indexed note count during CATCHING_UP");

  require_index_ready(handle, "catch-up export test should eventually settle back to READY");

  kernel::index::inject_full_rescan_delay_ms(0, 0);
  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_export_diagnostics_reflects_rebuilding_without_fault() {
  const auto vault = make_temp_vault();
  const auto export_path = vault / "diagnostics-rebuilding.json";
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "rebuild diagnostics test should start from a ready state");

  kernel::index::inject_full_rescan_delay_ms(300, 1000);

  kernel_status rebuild_status{KERNEL_ERROR_INTERNAL};
  std::jthread rebuild_thread([&]() {
    rebuild_status = kernel_rebuild_index(handle);
  });

  require_eventually(
      [&]() {
        if (kernel_export_diagnostics(handle, export_path.string().c_str()).code != KERNEL_OK) {
          return false;
        }
        const std::string exported = read_file_text(export_path);
        return exported.find("\"index_state\":\"REBUILDING\"") != std::string::npos &&
               exported.find("\"index_fault_reason\":\"\"") != std::string::npos &&
               exported.find("\"index_fault_code\":0") != std::string::npos &&
               exported.find("\"index_fault_at_ns\":0") != std::string::npos;
      },
      "diagnostics should expose REBUILDING without fault fields during delayed rebuild");

  rebuild_thread.join();
  kernel::index::inject_full_rescan_delay_ms(0, 0);
  expect_ok(rebuild_status);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_export_diagnostics_reflects_initial_catch_up_fault_state() {
  const auto vault = make_temp_vault();
  const auto export_path = vault / "diagnostics-catch-up-fault.json";
  kernel::index::inject_full_rescan_failures(std::errc::io_error, 1000);

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_UNAVAILABLE;
      },
      "initial catch-up fault should degrade state before diagnostics export");

  expect_ok(kernel_export_diagnostics(handle, export_path.string().c_str()));
  const std::string exported = read_file_text(export_path);
  require_true(exported.find("\"index_state\":\"UNAVAILABLE\"") != std::string::npos, "diagnostics should reflect degraded catch-up state");
  require_true(exported.find("\"index_fault_reason\":\"initial_catch_up_failed\"") != std::string::npos, "diagnostics should expose initial catch-up failure reason");
  require_true(
      exported.find("\"index_fault_code\":" + std::to_string(std::make_error_code(std::errc::io_error).value())) != std::string::npos,
      "diagnostics should expose initial catch-up failure code");
  require_true(
      exported.find("\"index_fault_at_ns\":0") == std::string::npos,
      "diagnostics should expose a non-zero initial catch-up failure timestamp");

  kernel::index::inject_full_rescan_failures(std::errc::io_error, 0);
  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_rebuild_failure_sets_unavailable_and_exports_fault() {
  const auto vault = make_temp_vault();
  const auto export_path = vault / "diagnostics-rebuild-fault.json";
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "rebuild fault test should start from a ready state");

  kernel::index::inject_full_rescan_failures(std::errc::io_error, 1);
  const kernel_status rebuild_status = kernel_rebuild_index(handle);
  require_true(rebuild_status.code == KERNEL_ERROR_IO, "rebuild failure should surface as KERNEL_ERROR_IO");

  kernel_state_snapshot snapshot{};
  expect_ok(kernel_get_state(handle, &snapshot));
  require_true(snapshot.index_state == KERNEL_INDEX_UNAVAILABLE, "failed rebuild should leave index_state UNAVAILABLE");

  expect_ok(kernel_export_diagnostics(handle, export_path.string().c_str()));
  const std::string exported = read_file_text(export_path);
  require_true(exported.find("\"index_state\":\"UNAVAILABLE\"") != std::string::npos, "rebuild failure diagnostics should reflect degraded index state");
  require_true(exported.find("\"index_fault_reason\":\"rebuild_failed\"") != std::string::npos, "rebuild failure diagnostics should expose rebuild_failed");
  require_true(
      exported.find("\"index_fault_code\":" + std::to_string(std::make_error_code(std::errc::io_error).value())) != std::string::npos,
      "rebuild failure diagnostics should expose rebuild error code");
  require_true(
      exported.find("\"index_fault_at_ns\":0") == std::string::npos,
      "rebuild failure diagnostics should expose a non-zero failure timestamp");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_rebuild_recovers_ready_state_and_clears_fault_fields() {
  const auto vault = make_temp_vault();
  const auto export_path = vault / "diagnostics-rebuild-recovered.json";
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "rebuild recovery test should start from a ready state");

  kernel::index::inject_full_rescan_failures(std::errc::io_error, 1);
  const kernel_status failed_rebuild_status = kernel_rebuild_index(handle);
  require_true(failed_rebuild_status.code == KERNEL_ERROR_IO, "seed rebuild failure should fail with KERNEL_ERROR_IO");

  expect_ok(kernel_rebuild_index(handle));

  kernel_state_snapshot snapshot{};
  expect_ok(kernel_get_state(handle, &snapshot));
  require_true(snapshot.index_state == KERNEL_INDEX_READY, "successful rebuild retry should restore READY");

  expect_ok(kernel_export_diagnostics(handle, export_path.string().c_str()));
  const std::string exported = read_file_text(export_path);
  require_true(exported.find("\"index_state\":\"READY\"") != std::string::npos, "recovered rebuild diagnostics should export READY state");
  require_true(exported.find("\"index_fault_reason\":\"\"") != std::string::npos, "successful rebuild retry should clear fault reason");
  require_true(exported.find("\"index_fault_code\":0") != std::string::npos, "successful rebuild retry should clear fault code");
  require_true(exported.find("\"index_fault_at_ns\":0") != std::string::npos, "successful rebuild retry should clear fault timestamp");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_export_diagnostics_reports_last_rebuild_duration_after_delayed_failure() {
  const auto vault = make_temp_vault();
  const auto export_path = vault / "diagnostics-last-rebuild-failure-duration.json";
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "failed rebuild duration diagnostics test should start from a ready state");

  kernel::index::inject_full_rescan_delay_ms(300, 1);
  kernel::index::inject_full_rescan_failures(std::errc::io_error, 1);
  const kernel_status rebuild_status = kernel_rebuild_index(handle);
  require_true(
      rebuild_status.code == KERNEL_ERROR_IO,
      "failed rebuild duration diagnostics test should fail with KERNEL_ERROR_IO");
  kernel::index::inject_full_rescan_delay_ms(0, 0);

  expect_ok(kernel_export_diagnostics(handle, export_path.string().c_str()));
  const std::string exported = read_file_text(export_path);
  require_true(
      exported.find("\"last_rebuild_result\":\"failed\"") != std::string::npos,
      "diagnostics should export the latest failed rebuild result");
  require_true(
      exported.find("\"last_rebuild_duration_ms\":0") == std::string::npos,
      "diagnostics should export a non-zero rebuild duration after a delayed rebuild failure");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_export_diagnostics_preserves_last_rebuild_result_code_while_next_task_runs() {
  const auto vault = make_temp_vault();
  const auto export_path = vault / "diagnostics-last-rebuild-running.json";
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "running rebuild diagnostics code test should start from a ready state");

  expect_ok(kernel_start_rebuild_index(handle));
  expect_ok(kernel_wait_for_rebuild(handle, 5000));

  kernel::index::inject_full_rescan_delay_ms(300, 1000);
  expect_ok(kernel_start_rebuild_index(handle));

  require_eventually(
      [&]() {
        if (kernel_export_diagnostics(handle, export_path.string().c_str()).code != KERNEL_OK) {
          return false;
        }
        const std::string exported = read_file_text(export_path);
        return exported.find("\"rebuild_in_flight\":true") != std::string::npos &&
               exported.find("\"has_last_rebuild_result\":true") != std::string::npos &&
               exported.find("\"last_rebuild_result\":\"succeeded\"") != std::string::npos &&
               exported.find("\"last_rebuild_result_code\":0") != std::string::npos &&
               exported.find("\"rebuild_current_generation\":2") != std::string::npos &&
               exported.find("\"rebuild_last_completed_generation\":1") != std::string::npos &&
               exported.find("\"rebuild_current_started_at_ns\":0") == std::string::npos;
      },
      "diagnostics should preserve the last completed rebuild result code and expose a non-zero rebuild_current_started_at_ns while the next rebuild task is already in flight");

  expect_ok(kernel_wait_for_rebuild(handle, 5000));
  kernel::index::inject_full_rescan_delay_ms(0, 0);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_export_diagnostics_retains_fault_history_after_recovery() {
  const auto vault = make_temp_vault();
  const auto export_path = vault / "diagnostics-fault-history.json";
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "fault history diagnostics test should start from a ready state");

  kernel::index::inject_full_rescan_failures(std::errc::io_error, 1);
  const kernel_status failed_rebuild_status = kernel_rebuild_index(handle);
  require_true(failed_rebuild_status.code == KERNEL_ERROR_IO, "seed rebuild failure should fail with KERNEL_ERROR_IO");

  expect_ok(kernel_rebuild_index(handle));
  expect_ok(kernel_export_diagnostics(handle, export_path.string().c_str()));
  const std::string exported = read_file_text(export_path);
  require_true(
      exported.find("\"index_fault_reason\":\"\"") != std::string::npos,
      "fault history diagnostics should clear the live fault after recovery");
  require_true(
      exported.find("\"index_fault_history\":[") != std::string::npos,
      "fault history diagnostics should export a fault history array");
  require_true(
      exported.find("\"reason\":\"rebuild_failed\"") != std::string::npos,
      "fault history diagnostics should retain the previous rebuild failure");
  require_true(
      exported.find("\"last_rebuild_result\":\"succeeded\"") != std::string::npos,
      "fault history diagnostics should reflect the latest successful rebuild result");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_initial_catch_up_failure_degrades_and_then_recovers_index_state() {
  const auto vault = make_temp_vault();
  kernel::index::inject_full_rescan_failures(std::errc::io_error, 1000);

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_UNAVAILABLE;
      },
      "initial catch-up failure should degrade index_state to UNAVAILABLE");

  kernel::index::inject_full_rescan_failures(std::errc::io_error, 0);

  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_READY;
      },
      "initial catch-up should recover to READY after later successful rescan");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_runtime_diagnostics_core_tests() {
  test_export_diagnostics_reports_recent_events_in_runtime_order();
  test_export_diagnostics_reports_last_continuity_fallback_reason();
  test_export_diagnostics_reports_recovery_outcome_after_corrupt_tail_cleanup();
  test_export_diagnostics_reports_temp_only_recovery_cleanup();
  test_export_diagnostics_reflects_catching_up_without_fault_and_without_stale_count();
  test_export_diagnostics_reflects_rebuilding_without_fault();
  test_export_diagnostics_reflects_initial_catch_up_fault_state();
  test_rebuild_failure_sets_unavailable_and_exports_fault();
  test_rebuild_recovers_ready_state_and_clears_fault_fields();
  test_export_diagnostics_reports_last_rebuild_duration_after_delayed_failure();
  test_export_diagnostics_preserves_last_rebuild_result_code_while_next_task_runs();
  test_export_diagnostics_retains_fault_history_after_recovery();
  test_initial_catch_up_failure_degrades_and_then_recovers_index_state();
}
