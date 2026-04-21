// Reason: This file locks the minimum FTS-backed search behavior before exposing any public search API.

#include "kernel/c_api.h"
#include "index/refresh.h"
#include "recovery/journal.h"
#include "search/search.h"
#include "storage/storage.h"
#include "support/test_support.h"
#include "third_party/sqlite/sqlite3.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

kernel::storage::Database open_search_db(const std::filesystem::path& vault) {
  kernel::storage::Database db;
  const std::error_code ec = kernel::storage::open_or_create(storage_db_for_vault(vault), db);
  require_true(!ec, "search db should open");
  return db;
}

void test_search_finds_written_note_by_body_term() {
  const auto vault = make_temp_vault();

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Search Title\n"
      "Unique body token: benzaldehyde.\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "search.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_close(handle));

  auto db = open_search_db(vault);
  std::vector<kernel::search::SearchHit> hits;
  const std::error_code ec = kernel::search::search_notes(db, "benzaldehyde", hits);
  require_true(!ec, "search should succeed");
  require_true(hits.size() == 1, "search should return one matching note");
  require_true(hits[0].rel_path == "search.md", "search hit should preserve rel_path");
  require_true(hits[0].title == "Search Title", "search hit should preserve parser title");
  kernel::storage::close(db);

  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_search_rewrite_replaces_old_fts_rows() {
  const auto vault = make_temp_vault();

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata first{};
  kernel_write_disposition disposition{};
  const std::string original = "# Title\nalpha token\n";
  expect_ok(kernel_write_note(
      handle,
      "rewrite-search.md",
      original.data(),
      original.size(),
      nullptr,
      &first,
      &disposition));

  kernel_note_metadata second{};
  const std::string rewritten = "# Title\nbeta token\n";
  expect_ok(kernel_write_note(
      handle,
      "rewrite-search.md",
      rewritten.data(),
      rewritten.size(),
      first.content_revision,
      &second,
      &disposition));
  expect_ok(kernel_close(handle));

  auto db = open_search_db(vault);
  std::vector<kernel::search::SearchHit> hits;
  std::error_code ec = kernel::search::search_notes(db, "alpha", hits);
  require_true(!ec, "search should succeed for old token query");
  require_true(hits.empty(), "rewrite should remove old FTS rows");

  ec = kernel::search::search_notes(db, "beta", hits);
  require_true(!ec, "search should succeed for new token query");
  require_true(hits.size() == 1, "rewrite should preserve only the new FTS row");
  require_true(hits[0].rel_path == "rewrite-search.md", "rewritten search hit should preserve rel_path");
  kernel::storage::close(db);

  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_search_no_op_preserves_single_fts_row() {
  const auto vault = make_temp_vault();

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content = "# Stable Title\nstable token\n";
  kernel_note_metadata first{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "stable.md",
      content.data(),
      content.size(),
      nullptr,
      &first,
      &disposition));
  require_true(disposition == KERNEL_WRITE_WRITTEN, "first write should persist the note");

  kernel_note_metadata second{};
  expect_ok(kernel_write_note(
      handle,
      "stable.md",
      content.data(),
      content.size(),
      first.content_revision,
      &second,
      &disposition));
  require_true(disposition == KERNEL_WRITE_NO_OP, "same-content rewrite should be NO_OP");
  expect_ok(kernel_close(handle));

  auto db = open_search_db(vault);
  std::vector<kernel::search::SearchHit> hits;
  const std::error_code ec = kernel::search::search_notes(db, "stable", hits);
  require_true(!ec, "search should succeed for stable token");
  require_true(hits.size() == 1, "no-op rewrite should not duplicate FTS hits");
  kernel::storage::close(db);

  sqlite3* readonly_db = open_sqlite_readonly(storage_db_for_vault(vault));
  require_true(
      query_single_int(readonly_db, "SELECT COUNT(*) FROM note_fts;") == 1,
      "no-op rewrite should keep exactly one note_fts row");
  sqlite3_close(readonly_db);

  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_search_startup_recovery_replaces_stale_fts_rows() {
  const auto vault = make_temp_vault();
  const auto state_dir = state_dir_for_vault(vault);
  const auto journal_path = journal_path_for_vault(vault);
  const auto target_path = vault / "recover-search.md";
  const auto temp_path = target_path.parent_path() / "recover-search.md.codex-recovery.tmp";

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata original_metadata{};
  kernel_write_disposition disposition{};
  const std::string original = "# Search Recovery\nalpha token\n";
  expect_ok(kernel_write_note(
      handle,
      "recover-search.md",
      original.data(),
      original.size(),
      nullptr,
      &original_metadata,
      &disposition));
  expect_ok(kernel_close(handle));

  const std::string recovered = "# Search Recovery\nbeta token\n";
  write_file_bytes(target_path, recovered);
  write_file_bytes(temp_path, "stale-temp");
  const std::error_code append_ec = kernel::recovery::append_save_begin(
      journal_path,
      "manual-search-recovery",
      "recover-search.md",
      temp_path);
  require_true(!append_ec, "manual SAVE_BEGIN append should succeed");

  handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  expect_ok(kernel_close(handle));

  auto db = open_search_db(vault);
  std::vector<kernel::search::SearchHit> hits;
  std::error_code ec = kernel::search::search_notes(db, "alpha", hits);
  require_true(!ec, "old-token search should succeed after recovery");
  require_true(hits.empty(), "startup recovery should replace stale FTS rows");

  ec = kernel::search::search_notes(db, "beta", hits);
  require_true(!ec, "new-token search should succeed after recovery");
  require_true(hits.size() == 1, "startup recovery should index recovered body text");
  require_true(hits[0].rel_path == "recover-search.md", "recovered hit should preserve rel_path");
  kernel::storage::close(db);

  sqlite3* readonly_db = open_sqlite_readonly(storage_db_for_vault(vault));
  require_true(
      query_single_int(readonly_db, "SELECT COUNT(*) FROM note_fts;") == 1,
      "startup recovery should keep a single note_fts row for the note");
  sqlite3_close(readonly_db);
  require_true(!std::filesystem::exists(temp_path), "startup recovery should clean stale temp file");

  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir);
}

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

void test_search_page_supports_offset_limit_and_exact_total_hits() {
  const auto vault = make_temp_vault();

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  for (int index = 0; index < 5; ++index) {
    const std::string rel_path = "page-" + std::to_string(index) + ".md";
    const std::string content =
        "# Page " + std::to_string(index) + "\nInternalPageToken " + std::to_string(index) + "\n";
    expect_ok(kernel_write_note(
        handle,
        rel_path.c_str(),
        content.data(),
        content.size(),
        nullptr,
        &metadata,
        &disposition));
  }
  expect_ok(kernel_close(handle));

  auto db = open_search_db(vault);
  kernel::search::SearchPage page;
  const std::error_code ec = kernel::search::search_notes(
      db,
      kernel::search::SearchQuery{"InternalPageToken", 2, 2},
      page);
  require_true(!ec, "internal paged search should succeed");
  require_true(page.hits.size() == 2, "internal paged search should return a middle page");
  require_true(page.total_hits == 5, "internal paged search should report the exact total hit count");
  require_true(page.has_more, "internal paged search should report that later pages exist");
  require_true(page.hits[0].rel_path == "page-2.md", "internal paged search should slice from the requested offset");
  require_true(page.hits[1].rel_path == "page-3.md", "internal paged search should preserve stable ordering");
  kernel::storage::close(db);

  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_search_page_returns_empty_page_when_offset_is_out_of_range() {
  const auto vault = make_temp_vault();

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "single-page.md",
      "# Single Page\nInternalOutOfRangeToken\n",
      37,
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_close(handle));

  auto db = open_search_db(vault);
  kernel::search::SearchPage page;
  const std::error_code ec = kernel::search::search_notes(
      db,
      kernel::search::SearchQuery{"InternalOutOfRangeToken", 2, 10},
      page);
  require_true(!ec, "out-of-range internal paged search should succeed");
  require_true(page.hits.empty(), "out-of-range internal paged search should return no hits");
  require_true(page.total_hits == 1, "out-of-range internal paged search should still report the exact total hit count");
  require_true(!page.has_more, "out-of-range internal paged search should report no more hits");
  kernel::storage::close(db);

  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_search_page_filters_notes_by_tag_and_path_prefix() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "lab");
  std::filesystem::create_directories(vault / "other");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string alpha =
      "# Alpha\n#chem\nInternalFilterToken alpha\n";
  const std::string beta =
      "# Beta\n#chem\nInternalFilterToken beta\n";
  const std::string untagged =
      "# Untagged\nInternalFilterToken untagged\n";
  const std::string gamma =
      "# Gamma\n#chem\nInternalFilterToken gamma\n";
  expect_ok(kernel_write_note(
      handle,
      "lab/alpha.md",
      alpha.data(),
      alpha.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_write_note(
      handle,
      "lab/beta.md",
      beta.data(),
      beta.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_write_note(
      handle,
      "lab/untagged.md",
      untagged.data(),
      untagged.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_write_note(
      handle,
      "other/gamma.md",
      gamma.data(),
      gamma.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_close(handle));

  auto db = open_search_db(vault);
  kernel::search::SearchQuery query{};
  query.query = "InternalFilterToken";
  query.limit = 8;
  query.kind = KERNEL_SEARCH_KIND_NOTE;
  query.tag_filter = "chem";
  query.path_prefix = "lab/";

  kernel::search::SearchPage page;
  const std::error_code ec = kernel::search::search_page(db, query, page);
  require_true(!ec, "filtered internal note search should succeed");
  require_true(page.hits.size() == 2, "filtered internal note search should return the tagged notes under the prefix");
  require_true(page.total_hits == 2, "filtered internal note search should report the exact filtered hit count");
  require_true(!page.has_more, "filtered internal note search should report no extra hits");
  require_true(page.hits[0].rel_path == "lab/alpha.md", "filtered internal note search should keep rel_path ordering");
  require_true(page.hits[1].rel_path == "lab/beta.md", "filtered internal note search should keep rel_path ordering");
  kernel::storage::close(db);

  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_search_page_finds_attachment_paths_and_marks_missing() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "assets");
  std::filesystem::create_directories(vault / "docs");
  write_file_bytes(vault / "assets" / "diagram.png", "png-bytes");
  write_file_bytes(vault / "docs" / "report.pdf", "pdf-bytes");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Attachment Search\n"
      "![Figure](assets/diagram.png)\n"
      "[Report](docs/report.pdf)\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "attachments-search.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_close(handle));

  auto db = open_search_db(vault);
  std::filesystem::remove(vault / "docs" / "report.pdf");
  const std::error_code refresh_ec =
      kernel::index::refresh_markdown_path(db, vault, "docs/report.pdf");
  require_true(!refresh_ec, "attachment delete refresh should succeed before attachment search");

  kernel::search::SearchQuery query{};
  query.query = "report";
  query.limit = 4;
  query.kind = KERNEL_SEARCH_KIND_ATTACHMENT;

  kernel::search::SearchPage page;
  const std::error_code ec = kernel::search::search_page(db, query, page);
  require_true(!ec, "internal attachment search should succeed");
  require_true(page.hits.size() == 1, "internal attachment search should return one path hit");
  require_true(page.total_hits == 1, "internal attachment search should report one total hit");
  require_true(page.hits[0].rel_path == "docs/report.pdf", "internal attachment search should preserve the attachment rel_path");
  require_true(page.hits[0].title == "report.pdf", "internal attachment search should expose the attachment basename as title");
  require_true(page.hits[0].match_flags == KERNEL_SEARCH_MATCH_PATH, "internal attachment search should report PATH matches");
  require_true(page.hits[0].snippet.empty(), "internal attachment search should not emit snippets");
  require_true(page.hits[0].snippet_status == KERNEL_SEARCH_SNIPPET_NONE, "internal attachment search should report no snippet state");
  require_true(page.hits[0].result_kind == KERNEL_SEARCH_RESULT_ATTACHMENT, "internal attachment search should return attachment hits");
  require_true(
      page.hits[0].result_flags == KERNEL_SEARCH_RESULT_FLAG_ATTACHMENT_MISSING,
      "internal attachment search should surface missing attachment state");
  kernel::storage::close(db);

  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_search_page_merges_all_kinds_with_notes_first_and_rel_path_order() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "all");
  write_file_bytes(vault / "all" / "mixedfiltertoken-00.png", "png-00");
  write_file_bytes(vault / "all" / "mixedfiltertoken-01.png", "png-01");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string first =
      "# A Mixed\n"
      "MixedFilterToken first note body\n"
      "![Figure](all/mixedfiltertoken-00.png)\n";
  const std::string second =
      "# B Mixed\n"
      "MixedFilterToken second note body\n"
      "![Figure](all/mixedfiltertoken-01.png)\n";
  expect_ok(kernel_write_note(
      handle,
      "all/a-note.md",
      first.data(),
      first.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_write_note(
      handle,
      "all/b-note.md",
      second.data(),
      second.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_close(handle));

  auto db = open_search_db(vault);
  kernel::search::SearchQuery query{};
  query.query = "MixedFilterToken";
  query.limit = 8;
  query.kind = KERNEL_SEARCH_KIND_ALL;
  query.path_prefix = "all/";

  kernel::search::SearchPage page;
  std::error_code ec = kernel::search::search_page(db, query, page);
  require_true(!ec, "all-kind internal search should succeed");
  require_true(page.hits.size() == 4, "all-kind internal search should return both notes and both attachments");
  require_true(page.total_hits == 4, "all-kind internal search should report the combined exact hit count");
  require_true(!page.has_more, "all-kind internal search should report no extra hits on the full page");
  require_true(page.hits[0].rel_path == "all/a-note.md", "all-kind internal search should list notes first");
  require_true(page.hits[1].rel_path == "all/b-note.md", "all-kind internal search should preserve note rel_path ordering");
  require_true(page.hits[2].rel_path == "all/mixedfiltertoken-00.png", "all-kind internal search should list attachments after notes");
  require_true(page.hits[3].rel_path == "all/mixedfiltertoken-01.png", "all-kind internal search should preserve attachment rel_path ordering");
  require_true(page.hits[0].result_kind == KERNEL_SEARCH_RESULT_NOTE, "all-kind internal search should tag note hits as notes");
  require_true(page.hits[2].result_kind == KERNEL_SEARCH_RESULT_ATTACHMENT, "all-kind internal search should tag attachment hits as attachments");

  query.limit = 2;
  query.offset = 1;
  ec = kernel::search::search_page(db, query, page);
  require_true(!ec, "all-kind internal paged search should succeed");
  require_true(page.hits.size() == 2, "all-kind internal paged search should return a cross-boundary page");
  require_true(page.total_hits == 4, "all-kind internal paged search should keep the exact combined hit count");
  require_true(page.has_more, "all-kind internal paged search should report more hits after the cross-boundary page");
  require_true(page.hits[0].rel_path == "all/b-note.md", "all-kind internal paged search should start from the requested offset");
  require_true(page.hits[1].rel_path == "all/mixedfiltertoken-00.png", "all-kind internal paged search should preserve notes-first ordering across pagination");
  kernel::storage::close(db);

  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_search_page_ranking_boosts_title_hits_before_body_only_hits() {
  const auto vault = make_temp_vault();

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string body_only =
      "# Generic Body Rank\n"
      "InternalRankToken appears only in the body\n";
  const std::string title_hit =
      "# InternalRankToken\n"
      "body text without the unique rank token\n";
  expect_ok(kernel_write_note(
      handle,
      "a-body-rank.md",
      body_only.data(),
      body_only.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_write_note(
      handle,
      "z-title-rank.md",
      title_hit.data(),
      title_hit.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_close(handle));

  auto db = open_search_db(vault);
  kernel::search::SearchQuery query{};
  query.query = "InternalRankToken";
  query.limit = 8;
  query.kind = KERNEL_SEARCH_KIND_NOTE;
  query.sort_mode = KERNEL_SEARCH_SORT_RANK_V1;

  kernel::search::SearchPage page;
  const std::error_code ec = kernel::search::search_page(db, query, page);
  require_true(!ec, "ranked internal note search should succeed");
  require_true(page.hits.size() == 2, "ranked internal note search should return both matching notes");
  require_true(page.hits[0].rel_path == "z-title-rank.md", "ranked internal note search should boost title hits ahead of body-only hits");
  require_true(page.hits[0].title_rank_hit, "ranked internal note search should mark the leading title hit");
  require_true(page.hits[1].rel_path == "a-body-rank.md", "ranked internal note search should keep the body-only hit after the title hit");
  require_true(!page.hits[1].title_rank_hit, "ranked internal note search should not mark the trailing body-only hit as a title boost");
  kernel::storage::close(db);

  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_search_page_ranking_boosts_single_token_exact_tag_hits() {
  const auto vault = make_temp_vault();

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string tagged =
      "# Tagged Rank\n"
      "#rankboost\n"
      "rankboost body token\n";
  const std::string untagged =
      "# Untagged Rank\n"
      "rankboost body token\n";
  expect_ok(kernel_write_note(
      handle,
      "b-tagged-rank.md",
      tagged.data(),
      tagged.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_write_note(
      handle,
      "a-untagged-rank.md",
      untagged.data(),
      untagged.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_close(handle));

  auto db = open_search_db(vault);
  kernel::search::SearchQuery query{};
  query.query = "rankboost";
  query.limit = 8;
  query.kind = KERNEL_SEARCH_KIND_NOTE;
  query.sort_mode = KERNEL_SEARCH_SORT_RANK_V1;

  kernel::search::SearchPage page;
  const std::error_code ec = kernel::search::search_page(db, query, page);
  require_true(!ec, "ranked internal tag-boost search should succeed");
  require_true(page.hits.size() == 2, "ranked internal tag-boost search should return both matching notes");
  require_true(page.hits[0].rel_path == "b-tagged-rank.md", "ranked internal tag-boost search should boost exact single-token tag matches");
  require_true(page.hits[0].tag_exact_rank_hit, "ranked internal tag-boost search should mark the boosted tag match");
  require_true(page.hits[1].rel_path == "a-untagged-rank.md", "ranked internal tag-boost search should leave the untagged note behind the boosted note");
  require_true(!page.hits[1].tag_exact_rank_hit, "ranked internal tag-boost search should not mark the untagged note");
  kernel::storage::close(db);

  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_search_page_ranking_tie_breaks_by_rel_path() {
  const auto vault = make_temp_vault();

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string content =
      "# Generic Rank Tie\n"
      "RankTieToken appears in the body only\n";
  expect_ok(kernel_write_note(
      handle,
      "b-rank-tie.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_write_note(
      handle,
      "a-rank-tie.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_close(handle));

  auto db = open_search_db(vault);
  kernel::search::SearchQuery query{};
  query.query = "RankTieToken";
  query.limit = 8;
  query.kind = KERNEL_SEARCH_KIND_NOTE;
  query.sort_mode = KERNEL_SEARCH_SORT_RANK_V1;

  kernel::search::SearchPage page;
  const std::error_code ec = kernel::search::search_page(db, query, page);
  require_true(!ec, "ranked internal tie-break search should succeed");
  require_true(page.hits.size() == 2, "ranked internal tie-break search should return both matching notes");
  require_true(page.hits[0].rel_path == "a-rank-tie.md", "ranked internal tie-break search should fall back to rel_path ordering");
  require_true(page.hits[1].rel_path == "b-rank-tie.md", "ranked internal tie-break search should fall back to rel_path ordering");
  kernel::storage::close(db);

  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_search_page_ranking_all_kind_ranks_note_branch_and_appends_attachments() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "rankall");
  write_file_bytes(vault / "rankall" / "rankalltoken-00.png", "png-00");
  write_file_bytes(vault / "rankall" / "rankalltoken-01.png", "png-01");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string body_note =
      "# Generic Rank All\n"
      "RankAllToken body match only\n"
      "![Figure](rankall/rankalltoken-00.png)\n";
  const std::string title_note =
      "# RankAllToken\n"
      "body text without the unique rank token\n"
      "![Figure](rankall/rankalltoken-01.png)\n";
  expect_ok(kernel_write_note(
      handle,
      "rankall/a-body-note.md",
      body_note.data(),
      body_note.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_write_note(
      handle,
      "rankall/z-title-note.md",
      title_note.data(),
      title_note.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_close(handle));

  auto db = open_search_db(vault);
  kernel::search::SearchQuery query{};
  query.query = "RankAllToken";
  query.limit = 8;
  query.kind = KERNEL_SEARCH_KIND_ALL;
  query.path_prefix = "rankall/";
  query.sort_mode = KERNEL_SEARCH_SORT_RANK_V1;

  kernel::search::SearchPage page;
  std::error_code ec = kernel::search::search_page(db, query, page);
  require_true(!ec, "ranked internal all-kind search should succeed");
  require_true(page.hits.size() == 4, "ranked internal all-kind search should return both notes and both attachments");
  require_true(page.hits[0].rel_path == "rankall/z-title-note.md", "ranked internal all-kind search should rank the note branch before attachments");
  require_true(page.hits[1].rel_path == "rankall/a-body-note.md", "ranked internal all-kind search should keep lower-ranked notes before attachments");
  require_true(page.hits[2].rel_path == "rankall/rankalltoken-00.png", "ranked internal all-kind search should append attachments after ranked notes");
  require_true(page.hits[3].rel_path == "rankall/rankalltoken-01.png", "ranked internal all-kind search should preserve attachment rel_path ordering");

  query.limit = 2;
  query.offset = 1;
  ec = kernel::search::search_page(db, query, page);
  require_true(!ec, "ranked internal all-kind pagination should succeed");
  require_true(page.hits.size() == 2, "ranked internal all-kind pagination should return a cross-boundary page");
  require_true(page.hits[0].rel_path == "rankall/a-body-note.md", "ranked internal all-kind pagination should continue from the ranked note branch");
  require_true(page.hits[1].rel_path == "rankall/rankalltoken-00.png", "ranked internal all-kind pagination should enter the attachment branch after notes");
  kernel::storage::close(db);

  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_search_page_ranking_rejects_attachment_sort_mode() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "assets");
  write_file_bytes(vault / "assets" / "rank-attachment.png", "png");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Rank Attachment Boundary\n"
      "![Figure](assets/rank-attachment.png)\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "rank-attachment.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_close(handle));

  auto db = open_search_db(vault);
  kernel::search::SearchQuery query{};
  query.query = "rank";
  query.limit = 4;
  query.kind = KERNEL_SEARCH_KIND_ATTACHMENT;
  query.sort_mode = KERNEL_SEARCH_SORT_RANK_V1;

  kernel::search::SearchPage page;
  const std::error_code ec = kernel::search::search_page(db, query, page);
  require_true(
      ec == std::make_error_code(std::errc::invalid_argument),
      "ranked internal attachment search should remain invalid in Batch 4");
  require_true(page.hits.empty(), "invalid ranked internal attachment search should not return hits");
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

void test_incremental_refresh_detects_external_create() {
  const auto vault = make_temp_vault();

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  expect_ok(kernel_close(handle));

  write_file_bytes(
      vault / "external-create.md",
      "# External Create\ncreated-token\n");

  auto db = open_search_db(vault);
  const std::error_code refresh_ec =
      kernel::index::refresh_markdown_path(db, vault, "external-create.md");
  require_true(!refresh_ec, "external create refresh should succeed");

  std::vector<kernel::search::SearchHit> hits;
  const std::error_code search_ec = kernel::search::search_notes(db, "created-token", hits);
  require_true(!search_ec, "external create search should succeed");
  require_true(hits.size() == 1, "external create refresh should index the new note");
  require_true(hits[0].rel_path == "external-create.md", "external create hit should preserve rel_path");
  kernel::storage::close(db);

  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_incremental_refresh_detects_external_modify() {
  const auto vault = make_temp_vault();

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string original =
      "# Before External Modify\n"
      "before-token\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "external-modify.md",
      original.data(),
      original.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_close(handle));

  write_file_bytes(
      vault / "external-modify.md",
      "# After External Modify\n"
      "after-token\n");

  auto db = open_search_db(vault);
  const std::error_code refresh_ec =
      kernel::index::refresh_markdown_path(db, vault, "external-modify.md");
  require_true(!refresh_ec, "external modify refresh should succeed");

  std::vector<kernel::search::SearchHit> hits;
  std::error_code search_ec = kernel::search::search_notes(db, "before-token", hits);
  require_true(!search_ec, "old-token search should succeed after external modify");
  require_true(hits.empty(), "external modify refresh should remove stale FTS rows");

  search_ec = kernel::search::search_notes(db, "after-token", hits);
  require_true(!search_ec, "new-token search should succeed after external modify");
  require_true(hits.size() == 1, "external modify refresh should index new file contents");

  sqlite3* readonly_db = open_sqlite_readonly(storage_db_for_vault(vault));
  require_true(
      query_single_text(readonly_db, "SELECT title FROM notes WHERE rel_path='external-modify.md';") == "After External Modify",
      "external modify refresh should replace parser-derived title");
  sqlite3_close(readonly_db);
  kernel::storage::close(db);

  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_incremental_refresh_detects_external_delete() {
  const auto vault = make_temp_vault();

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# External Delete\n"
      "delete-token\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "external-delete.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_close(handle));

  std::filesystem::remove(vault / "external-delete.md");

  auto db = open_search_db(vault);
  const std::error_code refresh_ec =
      kernel::index::refresh_markdown_path(db, vault, "external-delete.md");
  require_true(!refresh_ec, "external delete refresh should succeed");

  std::vector<kernel::search::SearchHit> hits;
  const std::error_code search_ec = kernel::search::search_notes(db, "delete-token", hits);
  require_true(!search_ec, "external delete search should succeed");
  require_true(hits.empty(), "external delete refresh should remove the note from search");

  sqlite3* readonly_db = open_sqlite_readonly(storage_db_for_vault(vault));
  require_true(
      query_single_int(readonly_db, "SELECT COUNT(*) FROM notes WHERE rel_path='external-delete.md' AND is_deleted=1;") == 1,
      "external delete refresh should mark the note deleted");
  require_true(
      query_single_int(readonly_db, "SELECT COUNT(*) FROM note_tags WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='external-delete.md');") == 0,
      "external delete refresh should clear derived tags");
  require_true(
      query_single_int(readonly_db, "SELECT COUNT(*) FROM note_links WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='external-delete.md');") == 0,
      "external delete refresh should clear derived links");
  require_true(
      query_single_int(readonly_db, "SELECT COUNT(*) FROM note_fts WHERE rowid=(SELECT note_id FROM notes WHERE rel_path='external-delete.md');") == 0,
      "external delete refresh should clear the FTS row");
  sqlite3_close(readonly_db);
  kernel::storage::close(db);

  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_incremental_refresh_marks_deleted_attachment_missing() {
  const auto vault = make_temp_vault();
  write_file_bytes(vault / "assets-diagram.png", "png-bytes");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  expect_ok(kernel_close(handle));

  auto db = open_search_db(vault);
  std::error_code refresh_ec =
      kernel::index::refresh_markdown_path(db, vault, "assets-diagram.png");
  require_true(!refresh_ec, "attachment create refresh should succeed");

  sqlite3* readonly_db = open_sqlite_readonly(storage_db_for_vault(vault));
  require_true(
      query_single_int(
          readonly_db,
          "SELECT COUNT(*) FROM attachments WHERE rel_path='assets-diagram.png' AND is_missing=0;") == 1,
      "attachment refresh should register a present attachment");
  sqlite3_close(readonly_db);

  std::filesystem::remove(vault / "assets-diagram.png");

  refresh_ec = kernel::index::refresh_markdown_path(db, vault, "assets-diagram.png");
  require_true(!refresh_ec, "attachment delete refresh should succeed");

  readonly_db = open_sqlite_readonly(storage_db_for_vault(vault));
  require_true(
      query_single_int(
          readonly_db,
          "SELECT COUNT(*) FROM attachments WHERE rel_path='assets-diagram.png' AND is_missing=1;") == 1,
      "attachment delete refresh should mark attachment missing");
  sqlite3_close(readonly_db);

  kernel::storage::close(db);
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

int main() {
  try {
    test_search_finds_written_note_by_body_term();
    test_search_rewrite_replaces_old_fts_rows();
    test_search_no_op_preserves_single_fts_row();
    test_search_startup_recovery_replaces_stale_fts_rows();
    test_search_treats_hyphenated_query_as_literal_text();
    test_search_rejects_whitespace_only_query();
    test_search_matches_multiple_literal_tokens_with_extra_whitespace();
    test_search_returns_hits_in_rel_path_order();
    test_search_returns_one_hit_per_note_even_with_repeated_term();
    test_search_matches_title_only_token();
    test_search_matches_filename_fallback_title_token();
    test_search_reports_title_and_body_match_flags();
    test_search_extracts_plaintext_body_snippet_without_title_heading();
    test_search_title_only_result_leaves_snippet_empty();
    test_search_snippet_respects_fixed_max_length();
    test_search_page_supports_offset_limit_and_exact_total_hits();
    test_search_page_returns_empty_page_when_offset_is_out_of_range();
    test_search_page_filters_notes_by_tag_and_path_prefix();
    test_search_page_finds_attachment_paths_and_marks_missing();
    test_search_page_merges_all_kinds_with_notes_first_and_rel_path_order();
    test_search_page_ranking_boosts_title_hits_before_body_only_hits();
    test_search_page_ranking_boosts_single_token_exact_tag_hits();
    test_search_page_ranking_tie_breaks_by_rel_path();
    test_search_page_ranking_all_kind_ranks_note_branch_and_appends_attachments();
    test_search_page_ranking_rejects_attachment_sort_mode();
    test_search_respects_limit();
    test_incremental_refresh_detects_external_create();
    test_incremental_refresh_detects_external_modify();
    test_incremental_refresh_detects_external_delete();
    test_incremental_refresh_marks_deleted_attachment_missing();
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "search_tests failed: " << ex.what() << "\n";
    return 1;
  }
}
