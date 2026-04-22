// Reason: Keep rebuild status snapshot coverage separate from rebuild execution lifecycle tests.

#include "kernel/c_api.h"

#include "api/kernel_api_rebuild_runtime_suites.h"
#include "api/kernel_api_test_support.h"
#include "index/refresh.h"
#include "support/test_support.h"

#include <filesystem>
#include <system_error>

namespace {

void test_get_rebuild_status_reports_idle_then_running_then_success() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "rebuild status success test should start from a ready state");

  kernel_rebuild_status_snapshot rebuild_status{};
  expect_ok(kernel_get_rebuild_status(handle, &rebuild_status));
  require_true(!rebuild_status.in_flight, "fresh runtime should report no rebuild in flight");
  require_true(
      !rebuild_status.has_last_result,
      "fresh runtime should report has_last_result=false before any background rebuild completes");
  require_true(
      rebuild_status.last_result_code == KERNEL_ERROR_NOT_FOUND,
      "fresh runtime should report no completed background rebuild result");

  kernel::index::inject_full_rescan_delay_ms(300, 1000);
  expect_ok(kernel_start_rebuild_index(handle));

  require_eventually(
      [&]() {
        kernel_rebuild_status_snapshot during{};
        if (kernel_get_rebuild_status(handle, &during).code != KERNEL_OK) {
          return false;
        }
        return during.in_flight &&
               !during.has_last_result &&
               during.last_result_code == KERNEL_ERROR_NOT_FOUND;
      },
      "rebuild status should report an in-flight background rebuild without a completed result yet");

  expect_ok(kernel_wait_for_rebuild(handle, 5000));
  kernel::index::inject_full_rescan_delay_ms(0, 0);

  expect_ok(kernel_get_rebuild_status(handle, &rebuild_status));
  require_true(!rebuild_status.in_flight, "completed rebuild should no longer be in flight");
  require_true(rebuild_status.has_last_result, "completed rebuild should report has_last_result=true");
  require_true(rebuild_status.last_result_code == KERNEL_OK, "completed rebuild should report last result KERNEL_OK");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_get_rebuild_status_reports_current_started_at_while_running() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "rebuild status current-start test should start from a ready state");

  kernel::index::inject_full_rescan_delay_ms(300, 1000);
  expect_ok(kernel_start_rebuild_index(handle));

  require_eventually(
      [&]() {
        kernel_rebuild_status_snapshot during{};
        if (kernel_get_rebuild_status(handle, &during).code != KERNEL_OK) {
          return false;
        }
        return during.in_flight && during.current_started_at_ns != 0;
      },
      "rebuild status should expose a non-zero current_started_at_ns while a background rebuild is running");

  expect_ok(kernel_wait_for_rebuild(handle, 5000));
  kernel::index::inject_full_rescan_delay_ms(0, 0);

  kernel_rebuild_status_snapshot after{};
  expect_ok(kernel_get_rebuild_status(handle, &after));
  require_true(!after.in_flight, "completed rebuild should no longer be in flight in current-start test");
  require_true(
      after.current_started_at_ns == 0,
      "completed rebuild should clear current_started_at_ns once no rebuild is in flight");
  require_true(after.last_result_at_ns != 0, "completed rebuild should still preserve the last result timestamp");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_get_rebuild_status_reports_monotonic_task_generations() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "rebuild generation test should start from a ready state");

  kernel_rebuild_status_snapshot rebuild_status{};
  expect_ok(kernel_get_rebuild_status(handle, &rebuild_status));
  require_true(
      rebuild_status.current_generation == 0 && rebuild_status.last_completed_generation == 0,
      "fresh runtime should report zero current and completed rebuild generations");

  kernel::index::inject_full_rescan_delay_ms(300, 1000);
  expect_ok(kernel_start_rebuild_index(handle));

  require_eventually(
      [&]() {
        kernel_rebuild_status_snapshot during{};
        if (kernel_get_rebuild_status(handle, &during).code != KERNEL_OK) {
          return false;
        }
        return during.in_flight && during.current_generation == 1 && during.last_completed_generation == 0;
      },
      "first background rebuild should report current_generation=1 while running");

  expect_ok(kernel_wait_for_rebuild(handle, 5000));
  kernel::index::inject_full_rescan_delay_ms(0, 0);

  expect_ok(kernel_get_rebuild_status(handle, &rebuild_status));
  require_true(
      !rebuild_status.in_flight &&
          rebuild_status.current_generation == 0 &&
          rebuild_status.last_completed_generation == 1,
      "completed first rebuild should clear current_generation and retain last_completed_generation=1");

  kernel::index::inject_full_rescan_delay_ms(300, 1000);
  expect_ok(kernel_start_rebuild_index(handle));

  require_eventually(
      [&]() {
        kernel_rebuild_status_snapshot during{};
        if (kernel_get_rebuild_status(handle, &during).code != KERNEL_OK) {
          return false;
        }
        return during.in_flight && during.current_generation == 2 && during.last_completed_generation == 1;
      },
      "second background rebuild should advance current_generation while preserving the last completed generation");

  expect_ok(kernel_wait_for_rebuild(handle, 5000));
  kernel::index::inject_full_rescan_delay_ms(0, 0);

  expect_ok(kernel_get_rebuild_status(handle, &rebuild_status));
  require_true(
      !rebuild_status.in_flight &&
          rebuild_status.current_generation == 0 &&
          rebuild_status.last_completed_generation == 2,
      "completed second rebuild should advance last_completed_generation to 2");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_get_rebuild_status_preserves_last_completed_result_while_next_task_runs() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "rebuild preserved-result test should start from a ready state");

  expect_ok(kernel_start_rebuild_index(handle));
  expect_ok(kernel_wait_for_rebuild(handle, 5000));

  kernel_rebuild_status_snapshot rebuild_status{};
  expect_ok(kernel_get_rebuild_status(handle, &rebuild_status));
  require_true(
      rebuild_status.has_last_result &&
          rebuild_status.last_result_code == KERNEL_OK &&
          rebuild_status.last_completed_generation == 1,
      "after one successful rebuild the status query should preserve the last completed success result");

  kernel::index::inject_full_rescan_delay_ms(300, 1000);
  expect_ok(kernel_start_rebuild_index(handle));

  require_eventually(
      [&]() {
        kernel_rebuild_status_snapshot during{};
        if (kernel_get_rebuild_status(handle, &during).code != KERNEL_OK) {
          return false;
        }
        return during.in_flight &&
               during.current_generation == 2 &&
               during.last_completed_generation == 1 &&
               during.has_last_result &&
               during.last_result_code == KERNEL_OK &&
               during.last_result_at_ns != 0;
      },
      "running a second rebuild should preserve the first rebuild's completed result while the new task is still in flight");

  expect_ok(kernel_wait_for_rebuild(handle, 5000));
  kernel::index::inject_full_rescan_delay_ms(0, 0);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_get_rebuild_status_reports_background_failure_result() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "rebuild status failure test should start from a ready state");

  kernel::index::inject_full_rescan_failures(std::errc::io_error, 1);
  expect_ok(kernel_start_rebuild_index(handle));
  const kernel_status wait_status = kernel_wait_for_rebuild(handle, 5000);
  require_true(
      wait_status.code == KERNEL_ERROR_IO,
      "background rebuild wait should surface failure in rebuild status failure test");

  kernel_rebuild_status_snapshot rebuild_status{};
  expect_ok(kernel_get_rebuild_status(handle, &rebuild_status));
  require_true(!rebuild_status.in_flight, "failed rebuild should no longer be in flight");
  require_true(
      rebuild_status.has_last_result,
      "failed rebuild should still report has_last_result=true after a completed task");
  require_true(
      rebuild_status.last_result_code == KERNEL_ERROR_IO,
      "failed rebuild should report the last background rebuild result code");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_rebuild_runtime_status_tests() {
  test_get_rebuild_status_reports_idle_then_running_then_success();
  test_get_rebuild_status_reports_current_started_at_while_running();
  test_get_rebuild_status_reports_monotonic_task_generations();
  test_get_rebuild_status_preserves_last_completed_result_while_next_task_runs();
  test_get_rebuild_status_reports_background_failure_result();
}
