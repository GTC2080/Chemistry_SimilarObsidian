// Reason: This file isolates expanded search snippet behavior so pagination coverage can stay separate.

#include "kernel/c_api.h"

#include "api/kernel_api_search_expanded_snippet_pagination_suites.h"
#include "api/kernel_api_search_public_surface_support.h"
#include "api/kernel_api_test_support.h"
#include "support/test_support.h"

#include <filesystem>
#include <string>

namespace {

void test_expanded_search_api_returns_body_snippet_and_exact_total_hits() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string a_text =
      "# A Expanded\n"
      "body contains ExpandedBodyToken near the first note\n";
  const std::string b_text =
      "# B Expanded\n"
      "body contains ExpandedBodyToken near the second note\n";
  expect_ok(kernel_write_note(handle, "b-expanded-body.md", b_text.data(), b_text.size(), nullptr, &metadata, &disposition));
  expect_ok(kernel_write_note(handle, "a-expanded-body.md", a_text.data(), a_text.size(), nullptr, &metadata, &disposition));

  kernel_search_query request = make_default_search_query("ExpandedBodyToken", 1);
  kernel_search_page page{};
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 1, "expanded search should honor the page limit");
  require_true(page.total_hits == 2, "expanded search should report an exact total hit count from the same query snapshot");
  require_true(page.has_more == 1, "expanded search should report has_more when more hits remain");
  require_true(
      std::string(page.hits[0].rel_path) == "a-expanded-body.md",
      "expanded search should preserve rel_path ordering while ranking is not enabled");
  require_true(
      std::string(page.hits[0].title) == "A Expanded",
      "expanded search should preserve titles");
  require_true(
      std::string(page.hits[0].snippet).find("ExpandedBodyToken") != std::string::npos,
      "expanded search should expose a body snippet containing the matching token");
  require_true(
      page.hits[0].snippet_status == KERNEL_SEARCH_SNIPPET_BODY_EXTRACTED,
      "expanded search should report BODY_EXTRACTED when the body snippet is available");
  require_true(
      page.hits[0].result_kind == KERNEL_SEARCH_RESULT_NOTE,
      "expanded search Batch 1 should return note hits");
  require_true(
      page.hits[0].result_flags == KERNEL_SEARCH_RESULT_FLAG_NONE,
      "expanded search Batch 1 should not set result flags for notes");
  require_true(
      page.hits[0].match_flags == KERNEL_SEARCH_MATCH_BODY,
      "expanded search should preserve body match flags");
  require_true(
      std::string(page.hits[0].snippet).find('\n') == std::string::npos,
      "expanded search body snippets should be plain text");
  kernel_free_search_page(&page);

  kernel_search_results legacy_results{};
  expect_ok(kernel_search_notes_limited(handle, "ExpandedBodyToken", 1, &legacy_results));
  require_true(legacy_results.count == 1, "legacy limited search should remain supported");
  require_true(
      std::string(legacy_results.hits[0].rel_path) == "a-expanded-body.md",
      "legacy limited search should keep the old rel_path ordering");
  require_true(
      legacy_results.hits[0].match_flags == KERNEL_SEARCH_MATCH_BODY,
      "legacy limited search should keep the old match flag behavior");
  kernel_free_search_results(&legacy_results);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_expanded_search_api_returns_title_only_without_snippet() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# ExpandedTitleOnlyToken\n"
      "body text without the unique title token\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "expanded-title-only.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));

  kernel_search_query request = make_default_search_query("ExpandedTitleOnlyToken", 10);
  kernel_search_page page{};
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 1, "expanded title-only search should return one hit");
  require_true(page.total_hits == 1, "expanded title-only search should report one total hit");
  require_true(page.has_more == 0, "expanded title-only search should report no remaining pages");
  require_true(
      page.hits[0].snippet_status == KERNEL_SEARCH_SNIPPET_TITLE_ONLY,
      "expanded title-only search should report TITLE_ONLY when no body snippet exists");
  require_true(
      std::string(page.hits[0].snippet).empty(),
      "expanded title-only search should leave the snippet empty");
  require_true(
      page.hits[0].match_flags == KERNEL_SEARCH_MATCH_TITLE,
      "expanded title-only search should preserve title match flags");
  kernel_free_search_page(&page);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_expanded_search_api_strips_title_heading_and_collapses_body_whitespace() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Expanded Snippet Title\n"
      "first line with    ExpandedSnippetToken\n"
      "\n"
      "second line after token\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "expanded-snippet.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));

  kernel_search_query request = make_default_search_query("ExpandedSnippetToken", 10);
  kernel_search_page page{};
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 1, "expanded snippet search should return one hit");
  require_true(
      std::string(page.hits[0].snippet).find("Expanded Snippet Title") == std::string::npos,
      "expanded snippet search should exclude the title heading from the body snippet");
  require_true(
      std::string(page.hits[0].snippet).find('\n') == std::string::npos,
      "expanded snippet search should collapse newlines");
  require_true(
      std::string(page.hits[0].snippet).find("  ") == std::string::npos,
      "expanded snippet search should collapse repeated whitespace");
  kernel_free_search_page(&page);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_search_expanded_snippet_tests() {
  test_expanded_search_api_returns_body_snippet_and_exact_total_hits();
  test_expanded_search_api_returns_title_only_without_snippet();
  test_expanded_search_api_strips_title_heading_and_collapses_body_whitespace();
}
