// Reason: Keep expanded snippet and pagination regressions separate from filter and ranking coverage.

#include "kernel/c_api.h"

#include "api/kernel_api_search_expanded_suites.h"
#include "api/kernel_api_search_public_surface_support.h"
#include "api/kernel_api_test_support.h"
#include "core/kernel_internal.h"
#include "search/search.h"
#include "support/test_support.h"

#include <filesystem>
#include <mutex>
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

void test_expanded_search_api_supports_exact_offset_limit_pagination() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  for (int index = 0; index < 5; ++index) {
    const std::string rel_path = "page-" + two_digit_index(index) + ".md";
    const std::string title = "# Page " + two_digit_index(index) + "\n";
    const std::string body = "ExpandedPageToken body " + std::to_string(index) + "\n";
    const std::string content = title + body;
    expect_ok(kernel_write_note(
        handle,
        rel_path.c_str(),
        content.data(),
        content.size(),
        nullptr,
        &metadata,
        &disposition));
  }

  kernel_search_query request = make_default_search_query("ExpandedPageToken", 2);
  kernel_search_page page{};
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 2, "first expanded page should return two hits");
  require_true(page.total_hits == 5, "first expanded page should report the exact total hit count");
  require_true(page.has_more == 1, "first expanded page should report has_more");
  require_true(std::string(page.hits[0].rel_path) == "page-00.md", "first expanded page should start at the first rel_path");
  require_true(std::string(page.hits[1].rel_path) == "page-01.md", "first expanded page should preserve rel_path order");
  kernel_free_search_page(&page);

  request.offset = 2;
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 2, "middle expanded page should return two hits");
  require_true(page.total_hits == 5, "middle expanded page should preserve the exact total hit count");
  require_true(page.has_more == 1, "middle expanded page should still report has_more");
  require_true(std::string(page.hits[0].rel_path) == "page-02.md", "middle expanded page should begin at the requested offset");
  require_true(std::string(page.hits[1].rel_path) == "page-03.md", "middle expanded page should preserve rel_path order");
  kernel_free_search_page(&page);

  request.offset = 4;
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 1, "last expanded page should return the remaining hit only");
  require_true(page.total_hits == 5, "last expanded page should preserve the exact total hit count");
  require_true(page.has_more == 0, "last expanded page should report no remaining hits");
  require_true(std::string(page.hits[0].rel_path) == "page-04.md", "last expanded page should return the last rel_path");
  kernel_free_search_page(&page);

  request.offset = 9;
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 0, "out-of-range expanded page should return no hits");
  require_true(page.total_hits == 5, "out-of-range expanded page should still expose the exact total hit count");
  require_true(page.has_more == 0, "out-of-range expanded page should report no remaining hits");
  kernel_free_search_page(&page);

  request.offset = 2;
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 2, "repeated expanded page query should still return two hits");
  require_true(std::string(page.hits[0].rel_path) == "page-02.md", "repeated expanded page query should preserve a stable first hit");
  require_true(std::string(page.hits[1].rel_path) == "page-03.md", "repeated expanded page query should preserve a stable second hit");
  kernel_free_search_page(&page);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_expanded_search_api_rejects_invalid_page_limits() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string content =
      "# Page Limit Test\n"
      "ExpandedLimitToken\n";
  expect_ok(kernel_write_note(
      handle,
      "page-limit.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));

  kernel_search_query request = make_default_search_query("ExpandedLimitToken", 1);
  kernel_search_page page{};
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 1, "page-limit setup should return one hit");

  request.limit = 0;
  require_true(
      kernel_query_search(handle, &request, &page).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "expanded search should reject zero page limits");
  require_true(
      page.hits == nullptr && page.count == 0 && page.total_hits == 0 && page.has_more == 0,
      "expanded search should clear stale output when zero page limit is rejected");

  request = make_default_search_query(
      "ExpandedLimitToken",
      kernel::search::kSearchPageMaxLimit + 1);
  require_true(
      kernel_query_search(handle, &request, &page).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "expanded search should reject page limits above the frozen maximum");
  require_true(
      page.hits == nullptr && page.count == 0 && page.total_hits == 0 && page.has_more == 0,
      "expanded search should clear stale output when over-max page limits are rejected");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_expanded_search_api_pagination_tracks_rewrite_and_rebuild() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  for (int index = 0; index < 4; ++index) {
    const std::string rel_path = "lifecycle-" + two_digit_index(index) + ".md";
    const std::string content =
        "# Lifecycle " + two_digit_index(index) + "\nExpandedLifecycleToken " + std::to_string(index) + "\n";
    expect_ok(kernel_write_note(
        handle,
        rel_path.c_str(),
        content.data(),
        content.size(),
        nullptr,
        &metadata,
        &disposition));
  }

  kernel_search_query request = make_default_search_query("ExpandedLifecycleToken", 2);
  request.offset = 1;
  kernel_search_page page{};
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 2, "pagination lifecycle setup should return a middle page");
  require_true(page.total_hits == 4, "pagination lifecycle setup should report four hits");
  require_true(std::string(page.hits[0].rel_path) == "lifecycle-01.md", "pagination lifecycle setup should expose the second hit at offset one");
  kernel_free_search_page(&page);

  kernel_owned_buffer existing_note{};
  kernel_note_metadata existing_metadata{};
  expect_ok(kernel_read_note(handle, "lifecycle-01.md", &existing_note, &existing_metadata));
  kernel_free_buffer(&existing_note);

  const std::string rewritten =
      "# Lifecycle 01\nno shared token now\n";
  expect_ok(kernel_write_note(
      handle,
      "lifecycle-01.md",
      rewritten.data(),
      rewritten.size(),
      existing_metadata.content_revision,
      &metadata,
      &disposition));

  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 2, "pagination should still return a full page after rewrite");
  require_true(page.total_hits == 3, "pagination should track the exact hit count after rewrite");
  require_true(std::string(page.hits[0].rel_path) == "lifecycle-02.md", "pagination should advance to the next rel_path after a rewrite removes one hit");
  kernel_free_search_page(&page);

  {
    std::lock_guard lock(handle->storage_mutex);
    exec_sql(
        handle->storage.connection,
        "DELETE FROM note_fts WHERE rowid=(SELECT note_id FROM notes WHERE rel_path='lifecycle-00.md');");
  }

  require_index_ready(handle, "pagination rebuild test should wait for READY before triggering rebuild");
  expect_ok(kernel_rebuild_index(handle));

  request.offset = 0;
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 2, "pagination should return the first rebuilt page");
  require_true(page.total_hits == 3, "pagination should restore the exact hit count after rebuild repairs drift");
  require_true(std::string(page.hits[0].rel_path) == "lifecycle-00.md", "pagination should restore rebuilt hits back into the first page");
  require_true(page.has_more == 1, "pagination should still report has_more after rebuild when more hits remain");
  kernel_free_search_page(&page);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_search_expanded_snippet_pagination_tests() {
  test_expanded_search_api_returns_body_snippet_and_exact_total_hits();
  test_expanded_search_api_returns_title_only_without_snippet();
  test_expanded_search_api_strips_title_heading_and_collapses_body_whitespace();
  test_expanded_search_api_supports_exact_offset_limit_pagination();
  test_expanded_search_api_rejects_invalid_page_limits();
  test_expanded_search_api_pagination_tracks_rewrite_and_rebuild();
}
