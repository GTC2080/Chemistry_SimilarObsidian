// Reason: Keep watcher fault and backoff diagnostics isolated from broader runtime diagnostics coverage.

#include "kernel/c_api.h"

#include "api/kernel_api_runtime_diagnostics_suites.h"
#include "api/kernel_api_test_support.h"
#include "core/kernel_internal.h"
#include "support/test_support.h"
#include "watcher/session.h"

#include <chrono>
#include <filesystem>
#include <string>
#include <thread>

namespace {

std::size_t count_occurrences(const std::string& haystack, const std::string& needle) {
  if (needle.empty()) {
    return 0;
  }

  std::size_t count = 0;
  std::size_t offset = 0;
  while ((offset = haystack.find(needle, offset)) != std::string::npos) {
    ++count;
    offset += needle.size();
  }
  return count;
}

void test_export_diagnostics_reflects_watcher_runtime_fault_state() {
  const auto vault = make_temp_vault();
  const auto export_path = vault / "diagnostics-fault.json";
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel::watcher::inject_next_poll_error(handle->watcher_session, std::errc::io_error);
  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_UNAVAILABLE;
      },
      "watcher fault should degrade state before diagnostics export");

  expect_ok(kernel_export_diagnostics(handle, export_path.string().c_str()));
  const std::string exported = read_file_text(export_path);
  require_true(exported.find("\"index_state\":\"UNAVAILABLE\"") != std::string::npos, "diagnostics should reflect degraded watcher state");
  require_true(exported.find("\"index_fault_reason\":\"watcher_poll_failed\"") != std::string::npos, "diagnostics should expose watcher poll failure reason");
  require_true(
      exported.find("\"index_fault_code\":" + std::to_string(std::make_error_code(std::errc::io_error).value())) != std::string::npos,
      "diagnostics should expose watcher poll failure code");
  require_true(
      exported.find("\"index_fault_at_ns\":0") == std::string::npos,
      "diagnostics should expose a non-zero watcher poll failure timestamp");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_background_watcher_error_sets_index_unavailable() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel::watcher::inject_next_poll_error(handle->watcher_session, std::errc::io_error);

  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        if (kernel_get_state(handle, &snapshot).code != KERNEL_OK) {
          return false;
        }
        return snapshot.index_state == KERNEL_INDEX_UNAVAILABLE;
      },
      "background watcher error should downgrade index_state to UNAVAILABLE");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_background_watcher_recovers_index_state_after_later_success() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel::watcher::inject_next_poll_error(handle->watcher_session, std::errc::io_error);

  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        if (kernel_get_state(handle, &snapshot).code != KERNEL_OK) {
          return false;
        }
        return snapshot.index_state == KERNEL_INDEX_UNAVAILABLE;
      },
      "background watcher error should first downgrade index_state");

  write_file_bytes(
      vault / "bg-recover.md",
      "# Background Recover\nbg-recover-token\n");

  require_eventually(
      [&]() {
        kernel_search_results results{};
        if (kernel_search_notes(handle, "bg-recover-token", &results).code != KERNEL_OK) {
          return false;
        }
        const bool indexed =
            results.count == 1 &&
            std::string(results.hits[0].rel_path) == "bg-recover.md";
        kernel_free_search_results(&results);
        if (!indexed) {
          return false;
        }

        kernel_state_snapshot snapshot{};
        if (kernel_get_state(handle, &snapshot).code != KERNEL_OK) {
          return false;
        }
        return snapshot.index_state == KERNEL_INDEX_READY;
      },
      "background watcher should recover index_state to READY after later successful polling");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_background_watcher_recovers_to_ready_without_external_work_and_clears_live_fault() {
  const auto vault = make_temp_vault();
  const auto export_path = vault / "diagnostics-watcher-recovered.json";
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel::watcher::inject_next_poll_error(handle->watcher_session, std::errc::io_error);

  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_UNAVAILABLE;
      },
      "transient watcher poll fault should first degrade index_state to UNAVAILABLE");

  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_READY;
      },
      "watcher should automatically recover to READY after a later healthy poll without external work");

  expect_ok(kernel_export_diagnostics(handle, export_path.string().c_str()));
  const std::string exported = read_file_text(export_path);
  require_true(
      exported.find("\"index_state\":\"READY\"") != std::string::npos,
      "diagnostics should export READY after automatic watcher recovery");
  require_true(
      exported.find("\"index_fault_reason\":\"\"") != std::string::npos,
      "diagnostics should clear the live watcher fault after automatic recovery");
  require_true(
      exported.find("\"index_fault_code\":0") != std::string::npos,
      "diagnostics should clear the live watcher fault code after automatic recovery");
  require_true(
      exported.find("\"index_fault_at_ns\":0") != std::string::npos,
      "diagnostics should clear the live watcher fault timestamp after automatic recovery");
  require_true(
      exported.find("\"reason\":\"watcher_poll_failed\"") != std::string::npos,
      "diagnostics should retain watcher poll failures in fault history after automatic recovery");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_repeated_watcher_poll_faults_do_not_duplicate_fault_history_before_recovery() {
  const auto vault = make_temp_vault();
  const auto export_path = vault / "diagnostics-watcher-fault-dedup.json";
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel::watcher::inject_next_poll_errors(handle->watcher_session, std::errc::io_error, 3);

  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_UNAVAILABLE;
      },
      "repeated watcher poll faults should first degrade index_state to UNAVAILABLE");

  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_READY;
      },
      "watcher should automatically recover to READY after repeated transient poll faults");

  expect_ok(kernel_export_diagnostics(handle, export_path.string().c_str()));
  const std::string exported = read_file_text(export_path);
  require_true(
      count_occurrences(exported, "\"reason\":\"watcher_poll_failed\"") == 1,
      "repeated identical watcher poll faults should collapse to one history record before recovery");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_repeated_watcher_poll_faults_back_off_before_auto_recovery() {
  const auto vault = make_temp_vault();
  const auto export_path = vault / "diagnostics-watcher-fault-backoff.json";
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const auto injected_at = std::chrono::steady_clock::now();
  kernel::watcher::inject_next_poll_errors(handle->watcher_session, std::errc::io_error, 3);

  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_UNAVAILABLE;
      },
      "repeated watcher poll faults should first degrade index_state to UNAVAILABLE");

  std::this_thread::sleep_for(std::chrono::milliseconds(40));

  kernel_state_snapshot snapshot{};
  expect_ok(kernel_get_state(handle, &snapshot));
  require_true(
      snapshot.index_state == KERNEL_INDEX_UNAVAILABLE,
      "repeated watcher poll faults should stay UNAVAILABLE for at least one backoff window before auto-recovery");

  expect_ok(kernel_export_diagnostics(handle, export_path.string().c_str()));
  const std::string exported = read_file_text(export_path);
  require_true(
      exported.find("\"index_fault_reason\":\"watcher_poll_failed\"") != std::string::npos,
      "diagnostics should keep the live watcher poll fault visible during backoff");

  std::chrono::steady_clock::time_point recovered_at{};
  require_eventually(
      [&]() {
        kernel_state_snapshot recovered{};
        const bool ready =
            kernel_get_state(handle, &recovered).code == KERNEL_OK &&
            recovered.index_state == KERNEL_INDEX_READY;
        if (ready) {
          recovered_at = std::chrono::steady_clock::now();
        }
        return ready;
      },
      "watcher should eventually auto-recover after the throttled retry window");
  require_true(
      std::chrono::duration_cast<std::chrono::milliseconds>(recovered_at - injected_at).count() >= 180,
      "repeated watcher poll faults should incur a retry backoff before READY is restored");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_close_interrupts_watcher_fault_backoff_promptly() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel::watcher::inject_next_poll_errors(handle->watcher_session, std::errc::io_error, 20);

  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_UNAVAILABLE;
      },
      "repeated watcher poll faults should first degrade index_state before close-backoff regression");

  const auto close_started_at = std::chrono::steady_clock::now();
  expect_ok(kernel_close(handle));
  const auto close_elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - close_started_at)
          .count();

  require_true(
      close_elapsed_ms < 150,
      "kernel_close should interrupt watcher fault backoff promptly instead of waiting through retry sleeps");

  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_runtime_diagnostics_watcher_tests() {
  test_export_diagnostics_reflects_watcher_runtime_fault_state();
  test_background_watcher_error_sets_index_unavailable();
  test_background_watcher_recovers_index_state_after_later_success();
  test_background_watcher_recovers_to_ready_without_external_work_and_clears_live_fault();
  test_repeated_watcher_poll_faults_do_not_duplicate_fault_history_before_recovery();
  test_repeated_watcher_poll_faults_back_off_before_auto_recovery();
  test_close_interrupts_watcher_fault_backoff_promptly();
}
