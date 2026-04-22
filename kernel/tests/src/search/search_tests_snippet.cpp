// Reason: Keep snippet regressions together because they lock the presentation payload of note search results.

#include "search/search_snippet_pagination_suites.h"

#include "kernel/c_api.h"
#include "search/search.h"
#include "search/search_test_support.h"
#include "support/test_support.h"

#include <filesystem>
#include <string>
#include <vector>

namespace {

using search_tests::open_search_db;

void test_search_extracts_plaintext_body_snippet_without_title_heading() {
  const auto vault = make_temp_vault();

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Internal Snippet Title\n"
      "first line with    SnippetBodyToken\n"
      "second line after token\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "internal-snippet.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_close(handle));

  auto db = open_search_db(vault);
  std::vector<kernel::search::SearchHit> hits;
  const std::error_code ec = kernel::search::search_notes(db, "SnippetBodyToken", hits);
  require_true(!ec, "body-snippet search should succeed");
  require_true(hits.size() == 1, "body-snippet search should return one hit");
  require_true(
      hits[0].snippet_status == KERNEL_SEARCH_SNIPPET_BODY_EXTRACTED,
      "body-snippet search should report BODY_EXTRACTED");
  require_true(
      hits[0].snippet.find("SnippetBodyToken") != std::string::npos,
      "body-snippet search should preserve the matching body token in the snippet");
  require_true(
      hits[0].snippet.find("Internal Snippet Title") == std::string::npos,
      "body-snippet search should exclude the title heading from the body snippet");
  require_true(
      hits[0].snippet.find('\n') == std::string::npos,
      "body-snippet search should collapse newlines into plain text");
  require_true(
      hits[0].snippet.find("  ") == std::string::npos,
      "body-snippet search should collapse repeated whitespace");
  kernel::storage::close(db);

  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_search_title_only_result_leaves_snippet_empty() {
  const auto vault = make_temp_vault();

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# InternalTitleOnlyToken\n"
      "body text without the unique title token\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "internal-title-only.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_close(handle));

  auto db = open_search_db(vault);
  std::vector<kernel::search::SearchHit> hits;
  const std::error_code ec = kernel::search::search_notes(db, "InternalTitleOnlyToken", hits);
  require_true(!ec, "title-only snippet search should succeed");
  require_true(hits.size() == 1, "title-only snippet search should return one hit");
  require_true(
      hits[0].snippet_status == KERNEL_SEARCH_SNIPPET_TITLE_ONLY,
      "title-only snippet search should report TITLE_ONLY");
  require_true(
      hits[0].snippet.empty(),
      "title-only snippet search should leave the snippet empty");
  kernel::storage::close(db);

  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_search_snippet_respects_fixed_max_length() {
  const auto vault = make_temp_vault();

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Internal Snippet Length\n"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa SnippetLengthToken "
      "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
      "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "internal-snippet-length.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_close(handle));

  auto db = open_search_db(vault);
  std::vector<kernel::search::SearchHit> hits;
  const std::error_code ec = kernel::search::search_notes(db, "SnippetLengthToken", hits);
  require_true(!ec, "snippet-length search should succeed");
  require_true(hits.size() == 1, "snippet-length search should return one hit");
  require_true(
      hits[0].snippet_status == KERNEL_SEARCH_SNIPPET_BODY_EXTRACTED,
      "snippet-length search should extract a body snippet");
  require_true(
      hits[0].snippet.find("SnippetLengthToken") != std::string::npos,
      "snippet-length search should keep the matching body token inside the snippet window");
  require_true(
      hits[0].snippet.size() <= kernel::search::kSearchSnippetMaxBytes,
      "snippet-length search should cap the snippet to the fixed maximum length");
  kernel::storage::close(db);

  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_search_snippet_tests() {
  test_search_extracts_plaintext_body_snippet_without_title_heading();
  test_search_title_only_result_leaves_snippet_empty();
  test_search_snippet_respects_fixed_max_length();
}
