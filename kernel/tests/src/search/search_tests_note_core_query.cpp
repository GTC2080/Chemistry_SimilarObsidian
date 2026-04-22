// Reason: Keep note-search query contract regressions together because they all lock the observable search semantics seen by callers.

#include "search/search_note_core_suites.h"

#include "kernel/c_api.h"
#include "search/search.h"
#include "search/search_test_support.h"
#include "support/test_support.h"

#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace {

using search_tests::open_search_db;

void test_search_treats_hyphenated_query_as_literal_text() {
  const auto vault = make_temp_vault();

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Hyphen Search\n"
      "body contains api-search-token for literal lookup\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "hyphen-search.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_close(handle));

  auto db = open_search_db(vault);
  std::vector<kernel::search::SearchHit> hits;
  const std::error_code ec = kernel::search::search_notes(db, "api-search-token", hits);
  require_true(!ec, "hyphenated query should not fail FTS parsing");
  require_true(hits.size() == 1, "hyphenated query should match the note as literal text");
  require_true(hits[0].rel_path == "hyphen-search.md", "hyphenated search hit should preserve rel_path");
  kernel::storage::close(db);

  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_search_rejects_whitespace_only_query() {
  const auto vault = make_temp_vault();

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Whitespace Search\n"
      "body contains stabletoken for setup\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "whitespace-search.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_close(handle));

  auto db = open_search_db(vault);
  std::vector<kernel::search::SearchHit> hits;
  const std::error_code ec = kernel::search::search_notes(db, "   \t  ", hits);
  require_true(ec == std::make_error_code(std::errc::invalid_argument), "whitespace-only query should be invalid");
  require_true(hits.empty(), "whitespace-only query should not return hits");
  kernel::storage::close(db);

  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_search_matches_multiple_literal_tokens_with_extra_whitespace() {
  const auto vault = make_temp_vault();

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Multi Token Search\n"
      "body contains alpha-token and beta-token together\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "multi-token-search.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_close(handle));

  auto db = open_search_db(vault);
  std::vector<kernel::search::SearchHit> hits;
  const std::error_code ec = kernel::search::search_notes(db, "  alpha-token   beta-token  ", hits);
  require_true(!ec, "multi-token literal query should succeed");
  require_true(hits.size() == 1, "multi-token literal query should require both tokens and match one note");
  require_true(hits[0].rel_path == "multi-token-search.md", "multi-token query should preserve rel_path");
  kernel::storage::close(db);

  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_search_returns_hits_in_rel_path_order() {
  const auto vault = make_temp_vault();

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content_b =
      "# B Title\n"
      "shared-order-token\n";
  kernel_note_metadata metadata_b{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "b-note.md",
      content_b.data(),
      content_b.size(),
      nullptr,
      &metadata_b,
      &disposition));

  const std::string content_a =
      "# A Title\n"
      "shared-order-token\n";
  kernel_note_metadata metadata_a{};
  expect_ok(kernel_write_note(
      handle,
      "a-note.md",
      content_a.data(),
      content_a.size(),
      nullptr,
      &metadata_a,
      &disposition));
  expect_ok(kernel_close(handle));

  auto db = open_search_db(vault);
  std::vector<kernel::search::SearchHit> hits;
  const std::error_code ec = kernel::search::search_notes(db, "shared-order-token", hits);
  require_true(!ec, "ordered search query should succeed");
  require_true(hits.size() == 2, "ordered search query should return two hits");
  require_true(hits[0].rel_path == "a-note.md", "search hits should be ordered by rel_path ascending");
  require_true(hits[1].rel_path == "b-note.md", "search hits should be ordered by rel_path ascending");
  kernel::storage::close(db);

  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_search_returns_one_hit_per_note_even_with_repeated_term() {
  const auto vault = make_temp_vault();

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Repeated Term\n"
      "repeat-token repeat-token repeat-token\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "repeat-note.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_close(handle));

  auto db = open_search_db(vault);
  std::vector<kernel::search::SearchHit> hits;
  const std::error_code ec = kernel::search::search_notes(db, "repeat-token", hits);
  require_true(!ec, "repeated-term search should succeed");
  require_true(hits.size() == 1, "a repeated term inside one note should still return one hit");
  require_true(hits[0].rel_path == "repeat-note.md", "repeated-term hit should preserve rel_path");
  kernel::storage::close(db);

  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_search_matches_title_only_token() {
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
      "title-only.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_close(handle));

  auto db = open_search_db(vault);
  std::vector<kernel::search::SearchHit> hits;
  const std::error_code ec = kernel::search::search_notes(db, "TitleOnlyToken", hits);
  require_true(!ec, "title-only query should succeed");
  require_true(hits.size() == 1, "title-only query should match the note");
  require_true(hits[0].rel_path == "title-only.md", "title-only query should preserve rel_path");
  kernel::storage::close(db);

  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_search_matches_filename_fallback_title_token() {
  const auto vault = make_temp_vault();

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "body text without heading\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "FallbackTitleToken.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_close(handle));

  auto db = open_search_db(vault);
  std::vector<kernel::search::SearchHit> hits;
  const std::error_code ec = kernel::search::search_notes(db, "FallbackTitleToken", hits);
  require_true(!ec, "filename-fallback title query should succeed");
  require_true(hits.size() == 1, "filename-fallback title query should match the note");
  require_true(hits[0].rel_path == "FallbackTitleToken.md", "filename-fallback title query should preserve rel_path");
  kernel::storage::close(db);

  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_search_reports_title_and_body_match_flags() {
  const auto vault = make_temp_vault();

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string title_only =
      "# TitleMatchOnlyToken\n"
      "body text without the special title token\n";
  const std::string body_only =
      "# Generic Title\n"
      "body contains BodyMatchOnlyToken\n";
  const std::string both =
      "# SharedMatchToken\n"
      "body also contains SharedMatchToken\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(handle, "title-match.md", title_only.data(), title_only.size(), nullptr, &metadata, &disposition));
  expect_ok(kernel_write_note(handle, "body-match.md", body_only.data(), body_only.size(), nullptr, &metadata, &disposition));
  expect_ok(kernel_write_note(handle, "both-match.md", both.data(), both.size(), nullptr, &metadata, &disposition));
  expect_ok(kernel_close(handle));

  auto db = open_search_db(vault);
  std::vector<kernel::search::SearchHit> hits;
  std::error_code ec = kernel::search::search_notes(db, "TitleMatchOnlyToken", hits);
  require_true(!ec, "title-match query should succeed");
  require_true(hits.size() == 1, "title-match query should return one hit");
  require_true(hits[0].match_flags == KERNEL_SEARCH_MATCH_TITLE, "title-only hit should report TITLE match only");

  ec = kernel::search::search_notes(db, "BodyMatchOnlyToken", hits);
  require_true(!ec, "body-match query should succeed");
  require_true(hits.size() == 1, "body-match query should return one hit");
  require_true(hits[0].match_flags == KERNEL_SEARCH_MATCH_BODY, "body-only hit should report BODY match only");

  ec = kernel::search::search_notes(db, "SharedMatchToken", hits);
  require_true(!ec, "shared-match query should succeed");
  require_true(hits.size() == 1, "shared-match query should return one hit");
  require_true(
      hits[0].match_flags == (KERNEL_SEARCH_MATCH_TITLE | KERNEL_SEARCH_MATCH_BODY),
      "shared-match hit should report TITLE and BODY flags");
  kernel::storage::close(db);

  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_search_respects_limit() {
  const auto vault = make_temp_vault();

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(handle, "b-limit.md", "# B\nlimit-token\n", 16, nullptr, &metadata, &disposition));
  expect_ok(kernel_write_note(handle, "a-limit.md", "# A\nlimit-token\n", 16, nullptr, &metadata, &disposition));
  expect_ok(kernel_close(handle));

  auto db = open_search_db(vault);
  std::vector<kernel::search::SearchHit> hits;
  const std::error_code ec = kernel::search::search_notes(db, "limit-token", 1, hits);
  require_true(!ec, "limited search should succeed");
  require_true(hits.size() == 1, "limit should cap the number of hits");
  require_true(hits[0].rel_path == "a-limit.md", "limit should preserve rel_path ordering");
  kernel::storage::close(db);

  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_search_note_query_contract_tests() {
  test_search_treats_hyphenated_query_as_literal_text();
  test_search_rejects_whitespace_only_query();
  test_search_matches_multiple_literal_tokens_with_extra_whitespace();
  test_search_returns_hits_in_rel_path_order();
  test_search_returns_one_hit_per_note_even_with_repeated_term();
  test_search_matches_title_only_token();
  test_search_matches_filename_fallback_title_token();
  test_search_reports_title_and_body_match_flags();
  test_search_respects_limit();
}
