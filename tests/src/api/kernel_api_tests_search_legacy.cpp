#include "kernel/c_api.h"
#include "api/kernel_api_search_public_surface_support.h"
#include "api/kernel_api_test_support.h"
#include "core/kernel_internal.h"
#include "recovery/journal.h"
#include "search/search.h"
#include "support/test_support.h"

#include <filesystem>
#include <mutex>
#include <string>

namespace {

void test_search_api_returns_matching_hits() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Search API Title\n"
      "Contains apisearchtoken in the body.\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "search-api.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));

  kernel_search_results results{};
  expect_ok(kernel_search_notes(handle, "apisearchtoken", &results));
  require_true(results.count == 1, "search API should return one hit");
  require_true(std::string(results.hits[0].rel_path) == "search-api.md", "search API should preserve rel_path");
  require_true(std::string(results.hits[0].title) == "Search API Title", "search API should preserve title");
  kernel_free_search_results(&results);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}


void test_search_api_rewrite_replaces_old_hits() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string original =
      "# Search Rewrite\n"
      "alphaapitoken\n";
  kernel_note_metadata first{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "search-rewrite.md",
      original.data(),
      original.size(),
      nullptr,
      &first,
      &disposition));

  const std::string rewritten =
      "# Search Rewrite\n"
      "betaapitoken\n";
  kernel_note_metadata second{};
  expect_ok(kernel_write_note(
      handle,
      "search-rewrite.md",
      rewritten.data(),
      rewritten.size(),
      first.content_revision,
      &second,
      &disposition));

  kernel_search_results results{};
  expect_ok(kernel_search_notes(handle, "alphaapitoken", &results));
  require_true(results.count == 0, "rewrite should remove old search hits");
  kernel_free_search_results(&results);

  expect_ok(kernel_search_notes(handle, "betaapitoken", &results));
  require_true(results.count == 1, "rewrite should keep the new search hit");
  require_true(std::string(results.hits[0].rel_path) == "search-rewrite.md", "rewrite hit should preserve rel_path");
  kernel_free_search_results(&results);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_search_api_accepts_hyphenated_literal_query() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Search API Hyphen\n"
      "Contains api-search-token in the body.\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "search-hyphen.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));

  kernel_search_results results{};
  expect_ok(kernel_search_notes(handle, "api-search-token", &results));
  require_true(results.count == 1, "hyphenated search API query should return one hit");
  require_true(std::string(results.hits[0].rel_path) == "search-hyphen.md", "hyphenated search hit should preserve rel_path");
  kernel_free_search_results(&results);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_search_api_rejects_whitespace_only_query() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Search API Whitespace\n"
      "Contains stabletoken in the body.\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "search-whitespace.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));

  kernel_search_results results{};
  const kernel_status status = kernel_search_notes(handle, "   \t  ", &results);
  require_true(status.code == KERNEL_ERROR_INVALID_ARGUMENT, "whitespace-only search API query should be invalid");
  require_true(results.count == 0, "invalid search API query should not return hits");
  require_true(results.hits == nullptr, "invalid search API query should not allocate hits");
  kernel_free_search_results(&results);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_search_api_matches_multiple_literal_tokens_with_extra_whitespace() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Search API Multi Token\n"
      "Contains alpha-token and beta-token in the body.\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "search-multi-token.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));

  kernel_search_results results{};
  expect_ok(kernel_search_notes(handle, "  alpha-token   beta-token  ", &results));
  require_true(results.count == 1, "multi-token search API query should return one hit");
  require_true(std::string(results.hits[0].rel_path) == "search-multi-token.md", "multi-token search hit should preserve rel_path");
  kernel_free_search_results(&results);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_search_api_returns_hits_in_rel_path_order() {
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

  kernel_search_results results{};
  expect_ok(kernel_search_notes(handle, "shared-order-token", &results));
  require_true(results.count == 2, "ordered search API query should return two hits");
  require_true(std::string(results.hits[0].rel_path) == "a-note.md", "search API hits should be ordered by rel_path ascending");
  require_true(std::string(results.hits[1].rel_path) == "b-note.md", "search API hits should be ordered by rel_path ascending");
  kernel_free_search_results(&results);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_search_api_returns_one_hit_per_note_even_with_repeated_term() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Repeated API Term\n"
      "repeat-token repeat-token repeat-token\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "repeat-api-note.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));

  kernel_search_results results{};
  expect_ok(kernel_search_notes(handle, "repeat-token", &results));
  require_true(results.count == 1, "repeated term inside one note should return one API hit");
  require_true(std::string(results.hits[0].rel_path) == "repeat-api-note.md", "repeated-term API hit should preserve rel_path");
  kernel_free_search_results(&results);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

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

void test_tag_query_returns_matching_notes_in_rel_path_order() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string a_text = "# A\n#sharedtag\n";
  const std::string b_text = "# B\n#sharedtag\n";
  const std::string c_text = "# C\n#othertag\n";

  expect_ok(kernel_write_note(handle, "b-note.md", a_text.data(), a_text.size(), nullptr, &metadata, &disposition));
  expect_ok(kernel_write_note(handle, "a-note.md", b_text.data(), b_text.size(), nullptr, &metadata, &disposition));
  expect_ok(kernel_write_note(handle, "c-note.md", c_text.data(), c_text.size(), nullptr, &metadata, &disposition));

  kernel_search_results results{};
  expect_ok(kernel_query_tag_notes(handle, "sharedtag", static_cast<size_t>(-1), &results));
  require_true(results.count == 2, "tag query should return two matching notes");
  require_true(std::string(results.hits[0].rel_path) == "a-note.md", "tag query should order hits by rel_path ascending");
  require_true(std::string(results.hits[0].title) == "B", "tag query should preserve note titles");
  require_true(results.hits[0].match_flags == KERNEL_SEARCH_MATCH_NONE, "tag query hits should report no TITLE/BODY flags");
  require_true(std::string(results.hits[1].rel_path) == "b-note.md", "tag query should order hits by rel_path ascending");
  require_true(std::string(results.hits[1].title) == "A", "tag query should preserve note titles");
  require_true(results.hits[1].match_flags == KERNEL_SEARCH_MATCH_NONE, "tag query hits should report no TITLE/BODY flags");
  kernel_free_search_results(&results);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_backlinks_query_returns_matching_sources() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string target_text = "# Target Note\nbody\n";
  const std::string source_a_text = "# Source A\n[[Target Note]]\n";
  const std::string source_b_text = "# Source B\n[[Target Note]]\n";
  const std::string unrelated_text = "# Unrelated\n[[Other Note]]\n";

  expect_ok(kernel_write_note(handle, "target.md", target_text.data(), target_text.size(), nullptr, &metadata, &disposition));
  expect_ok(kernel_write_note(handle, "b-source.md", source_a_text.data(), source_a_text.size(), nullptr, &metadata, &disposition));
  expect_ok(kernel_write_note(handle, "a-source.md", source_b_text.data(), source_b_text.size(), nullptr, &metadata, &disposition));
  expect_ok(kernel_write_note(handle, "c-unrelated.md", unrelated_text.data(), unrelated_text.size(), nullptr, &metadata, &disposition));

  kernel_search_results results{};
  expect_ok(kernel_query_backlinks(handle, "target.md", static_cast<size_t>(-1), &results));
  require_true(results.count == 2, "backlinks query should return two matching source notes");
  require_true(std::string(results.hits[0].rel_path) == "a-source.md", "backlinks query should order hits by rel_path ascending");
  require_true(std::string(results.hits[0].title) == "Source B", "backlinks query should preserve source titles");
  require_true(results.hits[0].match_flags == KERNEL_SEARCH_MATCH_NONE, "backlinks query hits should report no TITLE/BODY flags");
  require_true(std::string(results.hits[1].rel_path) == "b-source.md", "backlinks query should order hits by rel_path ascending");
  require_true(std::string(results.hits[1].title) == "Source A", "backlinks query should preserve source titles");
  require_true(results.hits[1].match_flags == KERNEL_SEARCH_MATCH_NONE, "backlinks query hits should report no TITLE/BODY flags");
  kernel_free_search_results(&results);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_backlinks_query_accepts_windows_style_relative_path() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string target_text = "# Nested Target\nbody\n";
  const std::string source_text = "# Nested Source\n[[Nested Target]]\n";

  expect_ok(kernel_write_note(handle, "nested/target.md", target_text.data(), target_text.size(), nullptr, &metadata, &disposition));
  expect_ok(kernel_write_note(handle, "nested/source.md", source_text.data(), source_text.size(), nullptr, &metadata, &disposition));

  kernel_search_results results{};
  expect_ok(kernel_query_backlinks(handle, "nested/target.md", static_cast<size_t>(-1), &results));
  require_true(results.count == 1, "forward-slash backlinks query should find the nested source");
  require_true(std::string(results.hits[0].rel_path) == "nested/source.md", "forward-slash backlinks query should preserve source rel_path");

  expect_ok(kernel_query_backlinks(handle, "nested\\target.md", static_cast<size_t>(-1), &results));
  require_true(results.count == 1, "backlinks query should normalize Windows-style separators");
  require_true(std::string(results.hits[0].rel_path) == "nested/source.md", "normalized backlinks query should preserve source rel_path");
  require_true(std::string(results.hits[0].title) == "Nested Source", "normalized backlinks query should preserve source title");
  kernel_free_search_results(&results);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_tag_and_backlinks_queries_follow_rewrite_recovery_and_rebuild() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string target_text = "# Stable Target\nbody\n";
  const std::string original_source_text = "# Source\n#oldtag\n[[Stable Target]]\n";
  expect_ok(kernel_write_note(handle, "target.md", target_text.data(), target_text.size(), nullptr, &metadata, &disposition));
  expect_ok(kernel_write_note(handle, "source.md", original_source_text.data(), original_source_text.size(), nullptr, &metadata, &disposition));

  const std::string rewritten_source_text = "# Source\n#newtag\n[[Other Target]]\n";
  expect_ok(kernel_write_note(
      handle,
      "source.md",
      rewritten_source_text.data(),
      rewritten_source_text.size(),
      metadata.content_revision,
      &metadata,
      &disposition));

  kernel_search_results results{};
  expect_ok(kernel_query_tag_notes(handle, "oldtag", static_cast<size_t>(-1), &results));
  require_true(results.count == 0, "rewrite should remove old tag query hits");
  kernel_free_search_results(&results);
  expect_ok(kernel_query_backlinks(handle, "target.md", static_cast<size_t>(-1), &results));
  require_true(results.count == 0, "rewrite should remove stale backlinks query hits");
  kernel_free_search_results(&results);

  {
    std::lock_guard lock(handle->storage_mutex);
    exec_sql(handle->storage.connection, "DELETE FROM note_tags WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='source.md');");
    exec_sql(handle->storage.connection, "INSERT INTO note_tags(note_id, tag) VALUES((SELECT note_id FROM notes WHERE rel_path='source.md'), 'staletag');");
    exec_sql(handle->storage.connection, "DELETE FROM note_links WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='source.md');");
    exec_sql(handle->storage.connection, "INSERT INTO note_links(note_id, target) VALUES((SELECT note_id FROM notes WHERE rel_path='source.md'), 'Stable Target');");
  }

  expect_ok(kernel_rebuild_index(handle));

  expect_ok(kernel_query_tag_notes(handle, "staletag", static_cast<size_t>(-1), &results));
  require_true(results.count == 0, "rebuild should remove stale tag query hits");
  kernel_free_search_results(&results);
  expect_ok(kernel_query_tag_notes(handle, "newtag", static_cast<size_t>(-1), &results));
  require_true(results.count == 1, "rebuild should restore live tag query hits");
  kernel_free_search_results(&results);
  expect_ok(kernel_query_backlinks(handle, "target.md", static_cast<size_t>(-1), &results));
  require_true(results.count == 0, "rebuild should remove stale backlink query hits");
  kernel_free_search_results(&results);

  expect_ok(kernel_close(handle));

  prepare_state_dir_for_vault(vault);
  const auto journal_path = journal_path_for_vault(vault);
  const auto temp_path = vault / "source.md.recovery.tmp";
  const std::string recovered_source_text = "# Source\n#recovertag\n[[Stable Target]]\n";
  write_file_bytes(vault / "source.md", recovered_source_text);
  write_file_bytes(temp_path, "stale temp");
  require_true(
      !kernel::recovery::append_save_begin(
          journal_path,
          "tag-backlink-recovery-op",
          "source.md",
          temp_path),
      "recovery journal append should succeed");

  handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "recovery-backed tag/backlink query test should settle to READY");

  expect_ok(kernel_query_tag_notes(handle, "recovertag", static_cast<size_t>(-1), &results));
  require_true(results.count == 1, "startup recovery should restore recovered tag query hits");
  kernel_free_search_results(&results);
  expect_ok(kernel_query_backlinks(handle, "target.md", static_cast<size_t>(-1), &results));
  require_true(results.count == 1, "startup recovery should restore recovered backlink query hits");
  require_true(std::string(results.hits[0].rel_path) == "source.md", "recovered backlink hit should preserve rel_path");
  kernel_free_search_results(&results);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_tag_query_rejects_invalid_inputs_and_supports_limit() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(handle, "b-note.md", "# B\n#limitag\n", 12, nullptr, &metadata, &disposition));
  expect_ok(kernel_write_note(handle, "a-note.md", "# A\n#limitag\n", 12, nullptr, &metadata, &disposition));

  kernel_search_results results{};
  require_true(
      kernel_query_tag_notes(handle, "", static_cast<size_t>(-1), &results).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "empty tag query should be invalid");
  require_true(
      kernel_query_tag_notes(handle, "   \t", static_cast<size_t>(-1), &results).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "whitespace-only tag query should be invalid");
  require_true(
      kernel_query_tag_notes(handle, "limitag", 0, &results).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "zero-limit tag query should be invalid");

  expect_ok(kernel_query_tag_notes(handle, "limitag", 1, &results));
  require_true(results.count == 1, "tag query limit should cap results");
  require_true(std::string(results.hits[0].rel_path) == "a-note.md", "tag query limit should keep rel_path ordering");
  require_true(std::string(results.hits[0].title) == "A", "tag query limit should preserve hit titles");
  require_true(results.hits[0].match_flags == KERNEL_SEARCH_MATCH_NONE, "tag query hits should report no TITLE/BODY flags");

  const kernel_status invalid_status = kernel_query_tag_notes(handle, "", static_cast<size_t>(-1), &results);
  const bool invalid_cleared = results.count == 0 && results.hits == nullptr;
  if (!invalid_cleared) {
    kernel_free_search_results(&results);
  }
  require_true(invalid_status.code == KERNEL_ERROR_INVALID_ARGUMENT, "invalid tag query should still report INVALID_ARGUMENT");
  require_true(invalid_cleared, "invalid tag query should clear stale output hits");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_backlinks_query_rejects_invalid_inputs_and_supports_limit() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string target_text = "# Target Note\nbody\n";
  const std::string source_a_text = "# Source A\n[[Target Note]]\n";
  const std::string source_b_text = "# Source B\n[[Target Note]]\n";

  expect_ok(kernel_write_note(handle, "target.md", target_text.data(), target_text.size(), nullptr, &metadata, &disposition));
  expect_ok(kernel_write_note(handle, "b-source.md", source_a_text.data(), source_a_text.size(), nullptr, &metadata, &disposition));
  expect_ok(kernel_write_note(handle, "a-source.md", source_b_text.data(), source_b_text.size(), nullptr, &metadata, &disposition));

  kernel_search_results results{};
  require_true(
      kernel_query_backlinks(handle, "", static_cast<size_t>(-1), &results).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "empty backlinks rel_path should be invalid");
  require_true(
      kernel_query_backlinks(handle, "..\\target.md", static_cast<size_t>(-1), &results).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "path-traversal backlinks rel_path should be invalid");
  require_true(
      kernel_query_backlinks(handle, "target.md", 0, &results).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "zero-limit backlinks query should be invalid");

  expect_ok(kernel_query_backlinks(handle, "target.md", 1, &results));
  require_true(results.count == 1, "backlinks query limit should cap results");
  require_true(std::string(results.hits[0].rel_path) == "a-source.md", "backlinks query limit should keep rel_path ordering");
  require_true(std::string(results.hits[0].title) == "Source B", "backlinks query limit should preserve source titles");
  require_true(results.hits[0].match_flags == KERNEL_SEARCH_MATCH_NONE, "backlinks query hits should report no TITLE/BODY flags");

  const kernel_status invalid_status = kernel_query_backlinks(handle, "", static_cast<size_t>(-1), &results);
  const bool invalid_cleared = results.count == 0 && results.hits == nullptr;
  if (!invalid_cleared) {
    kernel_free_search_results(&results);
  }
  require_true(invalid_status.code == KERNEL_ERROR_INVALID_ARGUMENT, "invalid backlinks query should still report INVALID_ARGUMENT");
  require_true(invalid_cleared, "invalid backlinks query should clear stale output hits");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}


}  // namespace

void run_search_legacy_tests() {
  test_search_api_returns_matching_hits();
  test_search_api_rewrite_replaces_old_hits();
  test_search_api_accepts_hyphenated_literal_query();
  test_search_api_rejects_whitespace_only_query();
  test_search_api_matches_multiple_literal_tokens_with_extra_whitespace();
  test_search_api_returns_hits_in_rel_path_order();
  test_search_api_returns_one_hit_per_note_even_with_repeated_term();
  test_search_api_matches_title_only_token();
  test_search_api_matches_filename_fallback_title_token();
  test_search_api_reports_title_and_body_match_flags();
  test_search_api_limited_query_rejects_invalid_inputs_and_supports_limit();
  test_tag_query_returns_matching_notes_in_rel_path_order();
  test_backlinks_query_returns_matching_sources();
  test_backlinks_query_accepts_windows_style_relative_path();
  test_tag_and_backlinks_queries_follow_rewrite_recovery_and_rebuild();
  test_tag_query_rejects_invalid_inputs_and_supports_limit();
  test_backlinks_query_rejects_invalid_inputs_and_supports_limit();
}
