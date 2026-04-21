// Reason: This file isolates legacy note-search title, flag, and limit surface coverage so basic matching tests stay smaller.

#include "kernel/c_api.h"

#include "api/kernel_api_search_legacy_note_suites.h"
#include "api/kernel_api_test_support.h"
#include "support/test_support.h"

#include <filesystem>
#include <string>

namespace {

void test_search_api_matches_title_only_token() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# TitleOnlyToken\n"
      "body text does not include the special title token\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "api-title-only.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));

  kernel_search_results results{};
  expect_ok(kernel_search_notes(handle, "TitleOnlyToken", &results));
  require_true(results.count == 1, "title-only API query should match one note");
  require_true(std::string(results.hits[0].rel_path) == "api-title-only.md", "title-only API query should preserve rel_path");
  kernel_free_search_results(&results);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_search_api_matches_filename_fallback_title_token() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "body text without heading\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "ApiFallbackTitleToken.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));

  kernel_search_results results{};
  expect_ok(kernel_search_notes(handle, "ApiFallbackTitleToken", &results));
  require_true(results.count == 1, "filename-fallback title API query should match one note");
  require_true(std::string(results.hits[0].rel_path) == "ApiFallbackTitleToken.md", "filename-fallback title API query should preserve rel_path");
  kernel_free_search_results(&results);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_search_api_reports_title_and_body_match_flags() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string title_only =
      "# ApiTitleOnlyToken\n"
      "body text without the special title token\n";
  const std::string body_only =
      "# Generic API Title\n"
      "body contains ApiBodyOnlyToken\n";
  const std::string both =
      "# ApiBothToken\n"
      "body also contains ApiBothToken\n";

  expect_ok(kernel_write_note(handle, "api-title-flag.md", title_only.data(), title_only.size(), nullptr, &metadata, &disposition));
  expect_ok(kernel_write_note(handle, "api-body-flag.md", body_only.data(), body_only.size(), nullptr, &metadata, &disposition));
  expect_ok(kernel_write_note(handle, "api-both-flag.md", both.data(), both.size(), nullptr, &metadata, &disposition));

  kernel_search_results results{};
  expect_ok(kernel_search_notes(handle, "ApiTitleOnlyToken", &results));
  require_true(results.count == 1, "title-only API query should return one hit");
  require_true(
      results.hits[0].match_flags == KERNEL_SEARCH_MATCH_TITLE,
      "title-only API query should report TITLE match only");
  kernel_free_search_results(&results);

  expect_ok(kernel_search_notes(handle, "ApiBodyOnlyToken", &results));
  require_true(results.count == 1, "body-only API query should return one hit");
  require_true(
      results.hits[0].match_flags == KERNEL_SEARCH_MATCH_BODY,
      "body-only API query should report BODY match only");
  kernel_free_search_results(&results);

  expect_ok(kernel_search_notes(handle, "ApiBothToken", &results));
  require_true(results.count == 1, "shared API query should return one hit");
  require_true(
      results.hits[0].match_flags == (KERNEL_SEARCH_MATCH_TITLE | KERNEL_SEARCH_MATCH_BODY),
      "shared API query should report TITLE and BODY matches");
  kernel_free_search_results(&results);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_search_api_limited_query_rejects_invalid_inputs_and_supports_limit() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(handle, "b-limit-search.md", "# B\nlimit-search-token\n", 23, nullptr, &metadata, &disposition));
  expect_ok(kernel_write_note(handle, "a-limit-search.md", "# A\nlimit-search-token\n", 23, nullptr, &metadata, &disposition));

  kernel_search_results results{};
  require_true(
      kernel_search_notes_limited(handle, "", 1, &results).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "empty limited search query should be invalid");
  require_true(
      kernel_search_notes_limited(handle, "limit-search-token", 0, &results).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "zero-limit search query should be invalid");

  expect_ok(kernel_search_notes_limited(handle, "limit-search-token", 1, &results));
  require_true(results.count == 1, "limited search should cap result count");
  require_true(std::string(results.hits[0].rel_path) == "a-limit-search.md", "limited search should keep rel_path ordering");
  require_true(std::string(results.hits[0].title) == "A", "limited search should preserve hit titles");
  require_true(results.hits[0].match_flags == KERNEL_SEARCH_MATCH_BODY, "limited search should preserve BODY match flags");

  const kernel_status invalid_status = kernel_search_notes_limited(handle, "", 1, &results);
  const bool invalid_cleared = results.count == 0 && results.hits == nullptr;
  if (!invalid_cleared) {
    kernel_free_search_results(&results);
  }
  require_true(invalid_status.code == KERNEL_ERROR_INVALID_ARGUMENT, "invalid limited search query should still report INVALID_ARGUMENT");
  require_true(invalid_cleared, "invalid limited search query should clear stale output hits");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_search_legacy_note_surface_tests() {
  test_search_api_matches_title_only_token();
  test_search_api_matches_filename_fallback_title_token();
  test_search_api_reports_title_and_body_match_flags();
  test_search_api_limited_query_rejects_invalid_inputs_and_supports_limit();
}
