// Reason: Pin the kernel-owned study/session store before the legacy Rust
// index.db study surface is retired.

#include "kernel/c_api.h"

#include "api/kernel_api_test_support.h"
#include "support/test_support.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace {

void require_ok_status(const kernel_status status, std::string_view context) {
  require_true(
      status.code == KERNEL_OK,
      std::string(context) + ": expected KERNEL_OK, got status " +
          std::to_string(status.code));
}

std::string buffer_to_string(const kernel_owned_buffer& buffer) {
  if (buffer.data == nullptr || buffer.size == 0) {
    return {};
  }
  return std::string(buffer.data, buffer.size);
}

std::int64_t now_epoch_secs() {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

void test_study_sessions_are_persisted_and_queried_by_kernel() {
  const auto vault = make_temp_vault();

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  std::int64_t session_id = 0;
  require_ok_status(
      kernel_start_study_session(handle, "lab/code.rs", "lab", &session_id),
      "start kernel study session");
  require_true(session_id > 0, "kernel study session should return a positive id");
  require_ok_status(
      kernel_tick_study_session(handle, session_id, 75),
      "tick kernel study session");
  require_ok_status(
      kernel_end_study_session(handle, session_id, 45),
      "end kernel study session");

  kernel_owned_buffer stats{};
  require_ok_status(
      kernel_query_study_stats_json(handle, now_epoch_secs(), 7, &stats),
      "query kernel study stats");
  const std::string stats_json = buffer_to_string(stats);
  require_true(
      stats_json.find("\"today_active_secs\":120") != std::string::npos,
      "kernel study stats should aggregate today's active seconds");
  require_true(
      stats_json.find("\"today_files\":1") != std::string::npos,
      "kernel study stats should aggregate distinct files");
  require_true(
      stats_json.find("\"folder\":\"lab\"") != std::string::npos,
      "kernel study stats should keep folder ranking in kernel storage");
  require_true(
      stats_json.find("\"note_id\":\"lab/code.rs\"") != std::string::npos,
      "kernel study stats should keep daily file details in kernel storage");
  kernel_free_buffer(&stats);

  kernel_owned_buffer truth{};
  require_ok_status(
      kernel_query_study_truth_state_json(handle, 1234567890000, &truth),
      "query kernel study truth state");
  const std::string truth_json = buffer_to_string(truth);
  require_true(
      truth_json.find("\"totalExp\":2") != std::string::npos,
      "kernel study truth state should be computed from stored activity");
  require_true(
      truth_json.find("\"lastSettlement\":1234567890000") != std::string::npos,
      "kernel study truth state should keep host-visible settlement timestamp");
  kernel_free_buffer(&truth);

  kernel_owned_buffer heatmap{};
  require_ok_status(
      kernel_query_study_heatmap_grid_json(handle, now_epoch_secs(), &heatmap),
      "query kernel study heatmap");
  const std::string heatmap_json = buffer_to_string(heatmap);
  require_true(
      heatmap_json.find("\"cells\"") != std::string::npos,
      "kernel study heatmap should return a precomputed grid");
  require_true(
      heatmap_json.find("\"maxSecs\"") != std::string::npos,
      "kernel study heatmap should include the maxSecs contract field");
  kernel_free_buffer(&heatmap);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_study_session_tests() {
  test_study_sessions_are_persisted_and_queried_by_kernel();
}
