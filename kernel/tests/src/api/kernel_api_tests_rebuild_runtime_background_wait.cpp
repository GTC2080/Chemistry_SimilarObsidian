// Reason: This file isolates background rebuild wait and final-result semantics so lifecycle coverage can stay separate.

#include "kernel/c_api.h"

#include "api/kernel_api_rebuild_runtime_background_suites.h"
#include "api/kernel_api_test_support.h"
#include "index/refresh.h"
#include "support/test_support.h"

#include <filesystem>
#include <string>
#include <system_error>

namespace {

void test_background_rebuild_join_is_idempotent_after_completion() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "background rebuild join-idempotence test should start from a ready state");

  kernel::index::inject_full_rescan_delay_ms(300, 1000);
  expect_ok(kernel_start_rebuild_index(handle));
  expect_ok(kernel_join_rebuild_index(handle));
  kernel::index::inject_full_rescan_delay_ms(0, 0);

  const kernel_status second_join = kernel_join_rebuild_index(handle);
  require_true(
      second_join.code == KERNEL_OK,
      "joining a completed background rebuild a second time should remain a harmless no-op");

  kernel_state_snapshot snapshot{};
  expect_ok(kernel_get_state(handle, &snapshot));
  require_true(snapshot.index_state == KERNEL_INDEX_READY, "second join should leave runtime state READY");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_background_rebuild_wait_times_out_while_work_is_in_flight() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "background rebuild wait-timeout test should start from a ready state");

  kernel::index::inject_full_rescan_delay_ms(300, 1000);
  expect_ok(kernel_start_rebuild_index(handle));

  const kernel_status wait_status = kernel_wait_for_rebuild(handle, 10);
  require_true(
      wait_status.code == KERNEL_ERROR_TIMEOUT,
      "background rebuild wait should report KERNEL_ERROR_TIMEOUT while delayed work is still in flight");

  kernel_state_snapshot snapshot{};
  expect_ok(kernel_get_state(handle, &snapshot));
  require_true(snapshot.index_state == KERNEL_INDEX_REBUILDING, "timed-out wait should leave runtime state REBUILDING");

  expect_ok(kernel_join_rebuild_index(handle));
  kernel::index::inject_full_rescan_delay_ms(0, 0);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_background_rebuild_wait_returns_final_result_after_completion() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "background rebuild wait-success test should start from a ready state");

  kernel::index::inject_full_rescan_delay_ms(300, 1000);
  expect_ok(kernel_start_rebuild_index(handle));

  const kernel_status wait_status = kernel_wait_for_rebuild(handle, 5000);
  require_true(
      wait_status.code == KERNEL_OK,
      "background rebuild wait should return final success once rebuild completes within timeout");
  kernel::index::inject_full_rescan_delay_ms(0, 0);

  kernel_state_snapshot snapshot{};
  expect_ok(kernel_get_state(handle, &snapshot));
  require_true(snapshot.index_state == KERNEL_INDEX_READY, "successful background rebuild wait should leave runtime state READY");

  const kernel_status second_wait = kernel_wait_for_rebuild(handle, 1);
  require_true(
      second_wait.code == KERNEL_OK,
      "re-waiting a completed successful background rebuild should preserve the final result");

  const kernel_status join_after_wait = kernel_join_rebuild_index(handle);
  require_true(
      join_after_wait.code == KERNEL_OK,
      "joining after a successful wait should preserve the same final result");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_background_rebuild_failure_result_remains_readable_after_completion() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "background rebuild failure retention test should start from a ready state");

  kernel::index::inject_full_rescan_failures(std::errc::io_error, 1);
  expect_ok(kernel_start_rebuild_index(handle));

  const kernel_status first_wait = kernel_wait_for_rebuild(handle, 5000);
  require_true(
      first_wait.code == KERNEL_ERROR_IO,
      "first wait should surface the background rebuild failure result");

  const kernel_status second_wait = kernel_wait_for_rebuild(handle, 1);
  require_true(
      second_wait.code == KERNEL_ERROR_IO,
      "re-waiting a completed failed background rebuild should preserve the failure result");

  const kernel_status join_after_wait = kernel_join_rebuild_index(handle);
  require_true(
      join_after_wait.code == KERNEL_ERROR_IO,
      "joining after a failed wait should preserve the same failure result");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_background_rebuild_wait_and_join_report_not_found_when_no_task_exists() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "idle rebuild task test should start from a ready state");

  const kernel_status wait_status = kernel_wait_for_rebuild(handle, 1);
  require_true(
      wait_status.code == KERNEL_ERROR_NOT_FOUND,
      "background rebuild wait should report KERNEL_ERROR_NOT_FOUND when no rebuild task exists");

  const kernel_status join_status = kernel_join_rebuild_index(handle);
  require_true(
      join_status.code == KERNEL_ERROR_NOT_FOUND,
      "background rebuild join should report KERNEL_ERROR_NOT_FOUND when no rebuild task exists");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_rebuild_runtime_background_wait_tests() {
  test_background_rebuild_join_is_idempotent_after_completion();
  test_background_rebuild_wait_times_out_while_work_is_in_flight();
  test_background_rebuild_wait_returns_final_result_after_completion();
  test_background_rebuild_failure_result_remains_readable_after_completion();
  test_background_rebuild_wait_and_join_report_not_found_when_no_task_exists();
}
