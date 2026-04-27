// Reason: Cover product compute rules at the kernel ABI boundary so Tauri
// command code can remain a thin bridge.

#include "kernel/c_api.h"

#include "support/test_support.h"

#include <string>
#include <string_view>

namespace {

void require_ok_status(const kernel_status status, std::string_view context) {
  require_true(
      status.code == KERNEL_OK,
      std::string(context) + ": expected KERNEL_OK, got status " +
          std::to_string(status.code));
}

std::string nullable_string(const char* value) {
  return value == nullptr ? std::string() : std::string(value);
}

std::string buffer_to_string(const kernel_owned_buffer& buffer) {
  if (buffer.data == nullptr || buffer.size == 0) {
    return {};
  }
  return std::string(buffer.data, buffer.size);
}

void test_truth_diff_text_delta_routes_by_extension() {
  const std::string prev = "a";
  const std::string curr(260, 'x');
  kernel_truth_diff_result result{};

  require_ok_status(
      kernel_compute_truth_diff(
          prev.data(),
          prev.size(),
          curr.data(),
          curr.size(),
          "csv",
          &result),
      "truth diff text delta");

  require_true(result.count == 1, "text delta should return one award");
  require_true(nullable_string(result.awards[0].attr) == "science", "csv delta routes to science");
  require_true(result.awards[0].amount == 5, "text delta should preserve legacy floor formula");
  require_true(
      result.awards[0].reason == KERNEL_TRUTH_AWARD_REASON_TEXT_DELTA,
      "text delta should return structured reason");
  require_true(result.awards[0].detail == nullptr, "text delta should not include detail");

  kernel_free_truth_diff_result(&result);
  require_true(result.awards == nullptr && result.count == 0, "truth diff free should reset result");
}

void test_truth_diff_code_language_award() {
  const std::string prev = "note";
  const std::string curr =
      "note\n"
      "```rust\n"
      "fn main() {}\n"
      "```";
  kernel_truth_diff_result result{};

  require_ok_status(
      kernel_compute_truth_diff(
          prev.data(),
          prev.size(),
          curr.data(),
          curr.size(),
          "md",
          &result),
      "truth diff code language");

  require_true(result.count == 1, "new rust code fence should return one award");
  require_true(nullable_string(result.awards[0].attr) == "engineering", "rust routes to engineering");
  require_true(result.awards[0].amount == 8, "code language award amount should match legacy rule");
  require_true(
      result.awards[0].reason == KERNEL_TRUTH_AWARD_REASON_CODE_LANGUAGE,
      "code award should return structured reason");
  require_true(nullable_string(result.awards[0].detail) == "rust", "code award should include language detail");

  kernel_free_truth_diff_result(&result);
}

void test_truth_diff_molecular_line_award() {
  const std::string prev = "atom-a";
  const std::string curr =
      "atom-a\n"
      "atom-b\n"
      "atom-c";
  kernel_truth_diff_result result{};

  require_ok_status(
      kernel_compute_truth_diff(
          prev.data(),
          prev.size(),
          curr.data(),
          curr.size(),
          "mol",
          &result),
      "truth diff molecular lines");

  require_true(result.count == 1, "molecular line growth should return one award");
  require_true(nullable_string(result.awards[0].attr) == "creation", "mol line growth routes to creation");
  require_true(result.awards[0].amount == 10, "molecular line award should scale by added lines");
  require_true(
      result.awards[0].reason == KERNEL_TRUTH_AWARD_REASON_MOLECULAR_EDIT,
      "molecular award should return structured reason");
  require_true(result.awards[0].detail == nullptr, "molecular award should not include detail");

  kernel_free_truth_diff_result(&result);
}

void test_truth_diff_empty_and_invalid_args() {
  const std::string curr = "content";
  kernel_truth_diff_result result{};

  require_ok_status(
      kernel_compute_truth_diff(nullptr, 0, curr.data(), curr.size(), "md", &result),
      "truth diff empty previous content");
  require_true(result.count == 0, "empty previous content should return no awards");

  require_true(
      kernel_compute_truth_diff(curr.data(), curr.size(), curr.data(), curr.size(), "md", nullptr)
              .code == KERNEL_ERROR_INVALID_ARGUMENT,
      "truth diff should reject null result");
  require_true(
      kernel_compute_truth_diff(nullptr, 1, curr.data(), curr.size(), "md", &result).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "truth diff should reject null non-empty previous buffer");
  require_true(
      kernel_compute_truth_diff(curr.data(), curr.size(), curr.data(), curr.size(), nullptr, &result)
              .code == KERNEL_ERROR_INVALID_ARGUMENT,
      "truth diff should reject null extension");
}

void test_semantic_context_trims_short_content() {
  const std::string content = "  short note  \n";
  kernel_owned_buffer buffer{};

  require_ok_status(
      kernel_build_semantic_context(content.data(), content.size(), &buffer),
      "semantic context short content");

  require_true(buffer_to_string(buffer) == "short note", "semantic context should trim short content");
  kernel_free_buffer(&buffer);
  require_true(buffer.data == nullptr && buffer.size == 0, "semantic context free should reset buffer");
}

void test_semantic_context_extracts_headings_and_recent_blocks() {
  const std::string content =
      "# Intro\n" + std::string(2300, 'x') +
      "\n\n## Keep 1\nalpha"
      "\n\n### Keep 2\nbeta"
      "\n\n#### Keep 3\ngamma"
      "\n\n# Keep 4\ndelta"
      "\n\nfinal block";
  const std::string expected =
      "Headings:\n"
      "## Keep 1\n"
      "### Keep 2\n"
      "#### Keep 3\n"
      "# Keep 4\n"
      "\n"
      "Recent focus:\n"
      "#### Keep 3\ngamma\n"
      "\n"
      "# Keep 4\ndelta\n"
      "\n"
      "final block";
  kernel_owned_buffer buffer{};

  require_ok_status(
      kernel_build_semantic_context(content.data(), content.size(), &buffer),
      "semantic context long content");

  require_true(buffer_to_string(buffer) == expected, "semantic context should preserve legacy focus shape");
  kernel_free_buffer(&buffer);
}

void test_semantic_context_validates_arguments() {
  const std::string content = "content";
  kernel_owned_buffer buffer{};

  require_true(
      kernel_build_semantic_context(content.data(), content.size(), nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "semantic context should reject null output");
  require_true(
      kernel_build_semantic_context(nullptr, 1, &buffer).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "semantic context should reject null non-empty content");
  require_ok_status(
      kernel_build_semantic_context(nullptr, 0, &buffer),
      "semantic context should accept empty null content");
  require_true(buffer.size == 0 && buffer.data == nullptr, "empty semantic context should return empty buffer");
}

void test_product_text_limits_are_kernel_owned() {
  size_t value = 0;

  require_ok_status(kernel_get_semantic_context_min_bytes(&value), "semantic context min bytes");
  require_true(value == 24, "semantic context min bytes should come from kernel");
  require_true(
      kernel_get_semantic_context_min_bytes(nullptr).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "semantic context min bytes should reject null output");

  require_ok_status(kernel_get_rag_context_per_note_char_limit(&value), "RAG context per note chars");
  require_true(value == 1500, "RAG context per-note char limit should come from kernel");
  require_true(
      kernel_get_rag_context_per_note_char_limit(nullptr).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "RAG context per-note char limit should reject null output");

  require_ok_status(kernel_get_embedding_text_char_limit(&value), "embedding text char limit");
  require_true(value == 2000, "embedding text char limit should come from kernel");
  require_true(
      kernel_get_embedding_text_char_limit(nullptr).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "embedding text char limit should reject null output");
}

void test_ai_host_runtime_defaults_are_kernel_owned() {
  size_t value = 0;

  require_ok_status(kernel_get_ai_chat_timeout_secs(&value), "AI chat timeout seconds");
  require_true(value == 120, "AI chat timeout should come from kernel");
  require_true(
      kernel_get_ai_chat_timeout_secs(nullptr).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "AI chat timeout should reject null output");

  require_ok_status(kernel_get_ai_ponder_timeout_secs(&value), "AI ponder timeout seconds");
  require_true(value == 60, "AI ponder timeout should come from kernel");
  require_true(
      kernel_get_ai_ponder_timeout_secs(nullptr).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "AI ponder timeout should reject null output");

  require_ok_status(
      kernel_get_ai_embedding_request_timeout_secs(&value),
      "AI embedding request timeout seconds");
  require_true(value == 30, "AI embedding request timeout should come from kernel");
  require_true(
      kernel_get_ai_embedding_request_timeout_secs(nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "AI embedding request timeout should reject null output");

  require_ok_status(kernel_get_ai_embedding_cache_limit(&value), "AI embedding cache limit");
  require_true(value == 64, "AI embedding cache limit should come from kernel");
  require_true(
      kernel_get_ai_embedding_cache_limit(nullptr).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "AI embedding cache limit should reject null output");

  require_ok_status(
      kernel_get_ai_embedding_concurrency_limit(&value),
      "AI embedding concurrency limit");
  require_true(value == 4, "AI embedding concurrency limit should come from kernel");
  require_true(
      kernel_get_ai_embedding_concurrency_limit(nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "AI embedding concurrency limit should reject null output");
}

void test_truth_state_routes_activity_and_levels() {
  const kernel_study_note_activity activities[] = {
      {"lab.csv", 120},
      {"code.rs", 3600},
      {"molecule.mol", 3000},
      {"ledger.base", 6000},
  };
  kernel_truth_state_snapshot state{};

  require_ok_status(
      kernel_compute_truth_state_from_activity(activities, 4, &state),
      "truth state from activity");

  require_true(state.level == 2, "truth state should compute overall level");
  require_true(state.total_exp == 112, "truth state should carry remaining level exp");
  require_true(state.next_level_exp == 150, "truth state should compute next level requirement");
  require_true(state.attribute_exp.science == 2, "csv activity routes to science exp");
  require_true(state.attribute_exp.engineering == 60, "rs activity routes to engineering exp");
  require_true(state.attribute_exp.creation == 50, "mol activity routes to creation exp");
  require_true(state.attribute_exp.finance == 100, "base activity routes to finance exp");
  require_true(state.attributes.science == 1, "science attribute level should use kernel curve");
  require_true(state.attributes.engineering == 2, "engineering attribute level should use kernel curve");
  require_true(state.attributes.creation == 2, "creation attribute level should use kernel curve");
  require_true(state.attributes.finance == 3, "finance attribute level should use kernel curve");
}

void test_truth_state_validates_arguments() {
  kernel_truth_state_snapshot state{};
  const kernel_study_note_activity invalid[] = {{nullptr, 60}};

  require_ok_status(
      kernel_compute_truth_state_from_activity(nullptr, 0, &state),
      "truth state should accept empty activity");
  require_true(state.level == 1, "empty truth state should start at level one");
  require_true(state.next_level_exp == 100, "empty truth state should keep level one requirement");
  require_true(
      kernel_compute_truth_state_from_activity(nullptr, 1, &state).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "truth state should reject null non-empty activity buffer");
  require_true(
      kernel_compute_truth_state_from_activity(invalid, 1, &state).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "truth state should reject null note ids");
  require_true(
      kernel_compute_truth_state_from_activity(nullptr, 0, nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "truth state should reject null output");
}

void test_study_stats_window_computes_legacy_boundaries() {
  kernel_study_stats_window window{};

  require_ok_status(
      kernel_compute_study_stats_window(1714305600, 7, &window),
      "study stats window");

  require_true(window.today_start_epoch_secs == 1714262400, "stats window should floor now to UTC day");
  require_true(window.today_bucket == 19841, "stats window should expose today bucket");
  require_true(window.week_start_epoch_secs == 1713744000, "stats window should preserve 6-day week lookback");
  require_true(window.daily_window_start_epoch_secs == 1713744000, "stats window should include days_back days");
  require_true(window.heatmap_start_epoch_secs == 1698796800, "stats window should preserve legacy heatmap lookback");
  require_true(window.folder_rank_limit == 5, "stats window should own folder ranking limit");
}

void test_study_stats_window_validates_arguments() {
  kernel_study_stats_window window{};

  require_true(
      kernel_compute_study_stats_window(1714305600, 0, &window).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "stats window should reject non-positive days_back");
  require_true(
      kernel_compute_study_stats_window(1714305600, 7, nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "stats window should reject null output");
}

void test_study_streak_counts_contiguous_active_days() {
  const int64_t buckets[] = {12, 10, 9, 10, 8, 2};
  int64_t streak = 0;

  require_ok_status(
      kernel_compute_study_streak_days(buckets, 6, 10, &streak),
      "study streak");

  require_true(streak == 3, "streak should count contiguous days through today");

  require_ok_status(
      kernel_compute_study_streak_days(buckets, 6, 11, &streak),
      "study streak missing today");
  require_true(streak == 0, "streak should be zero when today has no activity");
}

void test_study_streak_buckets_timestamps_in_kernel() {
  const int64_t timestamps[] = {
      10 * 86400 + 5,
      9 * 86400 + 120,
      8 * 86400,
      10 * 86400 + 400,
      12 * 86400,
      -1,
  };
  int64_t streak = 0;

  require_ok_status(
      kernel_compute_study_streak_days_from_timestamps(timestamps, 6, 10, &streak),
      "study streak from timestamps");
  require_true(streak == 3, "timestamp streak should bucket and dedupe active days");

  require_ok_status(
      kernel_compute_study_streak_days_from_timestamps(timestamps, 6, -1, &streak),
      "study streak from negative timestamp");
  require_true(streak == 1, "timestamp bucketing should floor negative epoch seconds");
}

void test_study_streak_validates_arguments() {
  int64_t streak = 0;

  require_ok_status(
      kernel_compute_study_streak_days(nullptr, 0, 10, &streak),
      "empty study streak");
  require_true(streak == 0, "empty streak should be zero");
  require_true(
      kernel_compute_study_streak_days(nullptr, 1, 10, &streak).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "streak should reject null non-empty bucket buffer");
  require_true(
      kernel_compute_study_streak_days(nullptr, 0, 10, nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "streak should reject null output");
  require_true(
      kernel_compute_study_streak_days_from_timestamps(nullptr, 1, 10, &streak).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "timestamp streak should reject null non-empty timestamp buffer");
  require_true(
      kernel_compute_study_streak_days_from_timestamps(nullptr, 0, 10, nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "timestamp streak should reject null output");
}

std::string heatmap_date_at(const kernel_heatmap_grid& grid, const size_t index) {
  return grid.cells[index].date == nullptr ? std::string() : std::string(grid.cells[index].date);
}

void test_study_heatmap_grid_builds_fixed_monday_aligned_grid() {
  const kernel_heatmap_day_activity days[] = {
      {"2023-10-30", 60},
      {"2024-01-01", 120},
      {"2024-01-01", 30},
      {"2024-04-28", 300},
      {"2022-01-01", 999},
  };
  kernel_heatmap_grid grid{};

  require_ok_status(
      kernel_build_study_heatmap_grid(days, 5, 1714305600, &grid),
      "study heatmap grid");

  require_true(grid.weeks == 26, "study heatmap grid should own week count");
  require_true(grid.days_per_week == 7, "study heatmap grid should own days per week");
  require_true(grid.count == 182, "study heatmap grid should return 26x7 cells");
  require_true(grid.max_secs == 300, "study heatmap grid should compute max seconds");
  require_true(heatmap_date_at(grid, 0) == "2023-10-30", "heatmap should start on Monday");
  require_true(grid.cells[0].secs == 60, "first cell should include matching activity");
  require_true(grid.cells[0].col == 0 && grid.cells[0].row == 0, "first cell coordinates");
  require_true(heatmap_date_at(grid, 63) == "2024-01-01", "duplicate activity date should be in grid");
  require_true(grid.cells[63].secs == 150, "duplicate activity dates should be summed");
  require_true(grid.cells[63].col == 9 && grid.cells[63].row == 0, "January first cell coordinates");
  require_true(heatmap_date_at(grid, 181) == "2024-04-28", "heatmap should end on today");
  require_true(grid.cells[181].secs == 300, "last cell should include today activity");
  require_true(grid.cells[181].col == 25 && grid.cells[181].row == 6, "last cell coordinates");

  kernel_free_study_heatmap_grid(&grid);
  require_true(grid.cells == nullptr && grid.count == 0, "heatmap free should reset grid");
}

void test_study_heatmap_grid_validates_arguments() {
  kernel_heatmap_grid grid{};
  const kernel_heatmap_day_activity invalid[] = {{nullptr, 60}};

  require_true(
      kernel_build_study_heatmap_grid(nullptr, 1, 1714305600, &grid).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "heatmap grid should reject null non-empty day buffer");
  require_true(
      kernel_build_study_heatmap_grid(invalid, 1, 1714305600, &grid).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "heatmap grid should reject null dates");
  require_true(
      kernel_build_study_heatmap_grid(nullptr, 0, 1714305600, nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "heatmap grid should reject null output");
}

}  // namespace

void run_product_compute_tests() {
  test_truth_diff_text_delta_routes_by_extension();
  test_truth_diff_code_language_award();
  test_truth_diff_molecular_line_award();
  test_truth_diff_empty_and_invalid_args();
  test_semantic_context_trims_short_content();
  test_semantic_context_extracts_headings_and_recent_blocks();
  test_semantic_context_validates_arguments();
  test_product_text_limits_are_kernel_owned();
  test_ai_host_runtime_defaults_are_kernel_owned();
  test_truth_state_routes_activity_and_levels();
  test_truth_state_validates_arguments();
  test_study_stats_window_computes_legacy_boundaries();
  test_study_stats_window_validates_arguments();
  test_study_streak_counts_contiguous_active_days();
  test_study_streak_buckets_timestamps_in_kernel();
  test_study_streak_validates_arguments();
  test_study_heatmap_grid_builds_fixed_monday_aligned_grid();
  test_study_heatmap_grid_validates_arguments();
}
