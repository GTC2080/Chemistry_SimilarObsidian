// Reason: This file isolates runtime rebuild diagnostics coverage so event and state diagnostics can remain compact.

#include "kernel/c_api.h"

#include "api/kernel_api_runtime_diagnostics_core_suites.h"
#include "api/kernel_api_test_support.h"
#include "index/refresh.h"
#include "support/test_support.h"

#include <filesystem>
#include <string>

namespace {

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

}  // namespace

void run_runtime_diagnostics_rebuild_tests() {
  test_rebuild_failure_sets_unavailable_and_exports_fault();
  test_rebuild_recovers_ready_state_and_clears_fault_fields();
  test_export_diagnostics_reports_last_rebuild_duration_after_delayed_failure();
  test_export_diagnostics_preserves_last_rebuild_result_code_while_next_task_runs();
  test_export_diagnostics_retains_fault_history_after_recovery();
}
