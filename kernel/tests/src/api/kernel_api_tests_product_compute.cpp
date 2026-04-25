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

}  // namespace

void run_product_compute_tests() {
  test_truth_diff_text_delta_routes_by_extension();
  test_truth_diff_code_language_award();
  test_truth_diff_molecular_line_award();
  test_truth_diff_empty_and_invalid_args();
}
