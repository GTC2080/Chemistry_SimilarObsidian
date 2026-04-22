// Reason: This file isolates runtime diagnostics state-transition coverage so rebuild result diagnostics can live separately.

#include "kernel/c_api.h"

#include "api/kernel_api_runtime_diagnostics_core_suites.h"
#include "api/kernel_api_test_support.h"
#include "index/refresh.h"
#include "support/test_support.h"

#include <filesystem>
#include <string>
#include <thread>

namespace {

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

  kernel::index::inject_full_rescan_delay_ms(800, 1000);

  kernel_status rebuild_status{KERNEL_ERROR_INTERNAL};
  std::jthread rebuild_thread([&]() {
    rebuild_status = kernel_rebuild_index(handle);
  });

  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_REBUILDING;
      },
      "delayed rebuild should expose REBUILDING before diagnostics export");

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

void run_runtime_diagnostics_state_tests() {
  test_export_diagnostics_reflects_catching_up_without_fault_and_without_stale_count();
  test_export_diagnostics_reflects_rebuilding_without_fault();
  test_export_diagnostics_reflects_initial_catch_up_fault_state();
  test_initial_catch_up_failure_degrades_and_then_recovers_index_state();
}
