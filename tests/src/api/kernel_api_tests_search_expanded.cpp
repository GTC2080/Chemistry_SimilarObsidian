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

void test_expanded_search_api_supports_note_tag_and_path_prefix_filters() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "notes");
  std::filesystem::create_directories(vault / "misc");
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string alpha =
      "# Alpha Filter\n"
      "#chem\n"
      "ExpandedFilterToken alpha\n";
  expect_ok(kernel_write_note(
      handle,
      "notes/alpha.md",
      alpha.data(),
      alpha.size(),
      nullptr,
      &metadata,
      &disposition));
  const std::string beta =
      "# Beta Filter\n"
      "#chem\n"
      "ExpandedFilterToken beta\n";
  expect_ok(kernel_write_note(
      handle,
      "notes/beta.md",
      beta.data(),
      beta.size(),
      nullptr,
      &metadata,
      &disposition));
  const std::string untagged =
      "# Untagged Filter\n"
      "ExpandedFilterToken untagged\n";
  expect_ok(kernel_write_note(
      handle,
      "notes/untagged.md",
      untagged.data(),
      untagged.size(),
      nullptr,
      &metadata,
      &disposition));
  const std::string outside_prefix =
      "# Outside Prefix\n"
      "#chem\n"
      "ExpandedFilterToken outside\n";
  expect_ok(kernel_write_note(
      handle,
      "misc/outside.md",
      outside_prefix.data(),
      outside_prefix.size(),
      nullptr,
      &metadata,
      &disposition));

  kernel_search_query request = make_default_search_query("ExpandedFilterToken", 10);
  request.tag_filter = "chem";
  request.path_prefix = "notes\\";
  kernel_search_page page{};
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 2, "expanded search should return only tagged notes inside the prefix");
  require_true(page.total_hits == 2, "expanded search should report the exact filtered hit count");
  require_true(page.has_more == 0, "expanded filtered note search should report no remaining pages");
  require_true(std::string(page.hits[0].rel_path) == "notes/alpha.md", "expanded filtered note search should keep rel_path ordering");
  require_true(std::string(page.hits[1].rel_path) == "notes/beta.md", "expanded filtered note search should keep rel_path ordering");
  require_true(page.hits[0].result_kind == KERNEL_SEARCH_RESULT_NOTE, "expanded filtered note search should keep note result kind");
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

void test_expanded_search_api_supports_attachment_path_hits_and_missing_flag() {
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
      "attachment-search.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_close(handle));

  std::filesystem::remove(vault / "docs" / "report.pdf");

  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "attachment search should wait for catch-up before querying missing attachment state");

  kernel_search_query request = make_default_search_query("report", 10);
  request.kind = KERNEL_SEARCH_KIND_ATTACHMENT;
  request.path_prefix = "docs\\";
  kernel_search_page page{};
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 1, "expanded attachment search should return the matching attachment path");
  require_true(page.total_hits == 1, "expanded attachment search should report the exact total hit count");
  require_true(page.has_more == 0, "expanded attachment search should report no remaining pages");
  require_true(
      std::string(page.hits[0].rel_path) == "docs/report.pdf",
      "expanded attachment search should preserve the attachment rel_path");
  require_true(
      std::string(page.hits[0].title) == "report.pdf",
      "expanded attachment search should expose the attachment basename as title");
  require_true(
      std::string(page.hits[0].snippet).empty(),
      "expanded attachment search should not emit a snippet");
  require_true(
      page.hits[0].snippet_status == KERNEL_SEARCH_SNIPPET_NONE,
      "expanded attachment search should report no snippet state");
  require_true(
      page.hits[0].match_flags == KERNEL_SEARCH_MATCH_PATH,
      "expanded attachment search should report PATH matches");
  require_true(
      page.hits[0].result_kind == KERNEL_SEARCH_RESULT_ATTACHMENT,
      "expanded attachment search should mark the hit as an attachment");
  require_true(
      page.hits[0].result_flags == KERNEL_SEARCH_RESULT_FLAG_ATTACHMENT_MISSING,
      "expanded attachment search should surface missing attachment state");
  kernel_free_search_page(&page);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_expanded_search_api_supports_kind_all_notes_first_then_attachments() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "all");
  write_file_bytes(vault / "all" / "expandedmixedtoken-00.png", "png-00");
  write_file_bytes(vault / "all" / "expandedmixedtoken-01.png", "png-01");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string first =
      "# A Mixed\n"
      "#chem\n"
      "ExpandedMixedToken first note body\n"
      "![Figure](all/expandedmixedtoken-00.png)\n";
  const std::string second =
      "# B Mixed\n"
      "#chem\n"
      "ExpandedMixedToken second note body\n"
      "![Figure](all/expandedmixedtoken-01.png)\n";
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

  kernel_search_query request = make_default_search_query("ExpandedMixedToken", 10);
  request.kind = KERNEL_SEARCH_KIND_ALL;
  request.tag_filter = "chem";
  request.path_prefix = "all/";
  kernel_search_page page{};
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 4, "expanded all-kind search should return both notes and both attachments");
  require_true(page.total_hits == 4, "expanded all-kind search should report the exact combined hit count");
  require_true(page.has_more == 0, "expanded all-kind search should report no remaining pages on the full result");
  require_true(std::string(page.hits[0].rel_path) == "all/a-note.md", "expanded all-kind search should list notes first");
  require_true(std::string(page.hits[1].rel_path) == "all/b-note.md", "expanded all-kind search should preserve note rel_path order");
  require_true(std::string(page.hits[2].rel_path) == "all/expandedmixedtoken-00.png", "expanded all-kind search should place attachments after notes");
  require_true(std::string(page.hits[3].rel_path) == "all/expandedmixedtoken-01.png", "expanded all-kind search should preserve attachment rel_path order");
  require_true(page.hits[0].result_kind == KERNEL_SEARCH_RESULT_NOTE, "expanded all-kind search should tag note hits correctly");
  require_true(page.hits[2].result_kind == KERNEL_SEARCH_RESULT_ATTACHMENT, "expanded all-kind search should tag attachment hits correctly");
  kernel_free_search_page(&page);

  request.limit = 2;
  request.offset = 1;
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 2, "expanded all-kind pagination should return a cross-boundary page");
  require_true(page.total_hits == 4, "expanded all-kind pagination should keep the exact combined hit count");
  require_true(page.has_more == 1, "expanded all-kind pagination should report more hits after the cross-boundary page");
  require_true(std::string(page.hits[0].rel_path) == "all/b-note.md", "expanded all-kind pagination should start at the requested offset");
  require_true(std::string(page.hits[1].rel_path) == "all/expandedmixedtoken-00.png", "expanded all-kind pagination should keep notes-first ordering");
  kernel_free_search_page(&page);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_expanded_search_api_rejects_invalid_filter_and_ranking_combinations_and_clears_stale_output() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "docs");
  write_file_bytes(vault / "docs" / "report.pdf", "pdf-bytes");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Filter Boundary\n"
      "#chem\n"
      "ExpandedBoundaryToken\n"
      "[Report](docs/report.pdf)\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "filter-boundary.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));

  kernel_search_query request = make_default_search_query("ExpandedBoundaryToken", 10);
  kernel_search_page page{};
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 1, "expanded filter boundary setup should return one note hit");

  request = make_default_search_query("report", 10);
  request.kind = KERNEL_SEARCH_KIND_ATTACHMENT;
  request.tag_filter = "chem";
  require_true(
      kernel_query_search(handle, &request, &page).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "expanded search should reject tag filters on attachment-only queries");
  require_true(
      page.hits == nullptr && page.count == 0 && page.total_hits == 0 && page.has_more == 0,
      "expanded search should clear stale output after rejecting attachment-plus-tag");

  request = make_default_search_query("ExpandedBoundaryToken", 10);
  request.path_prefix = "../notes/";
  require_true(
      kernel_query_search(handle, &request, &page).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "expanded search should reject invalid relative path prefixes");
  require_true(
      page.hits == nullptr && page.count == 0 && page.total_hits == 0 && page.has_more == 0,
      "expanded search should clear stale output after rejecting invalid path prefixes");

  request = make_default_search_query("ExpandedBoundaryToken", 10);
  request.include_deleted = 1;
  require_true(
      kernel_query_search(handle, &request, &page).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "expanded search should keep include_deleted disabled in Batch 4");

  request = make_default_search_query("report", 10);
  request.kind = KERNEL_SEARCH_KIND_ATTACHMENT;
  request.sort_mode = KERNEL_SEARCH_SORT_RANK_V1;
  require_true(
      kernel_query_search(handle, &request, &page).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "expanded search should reject attachment-only Ranking v1 requests");

  kernel_search_results legacy_results{};
  expect_ok(kernel_search_notes(handle, "ExpandedBoundaryToken", &legacy_results));
  require_true(legacy_results.count == 1, "legacy search should remain supported after invalid expanded-search requests");
  kernel_free_search_results(&legacy_results);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_expanded_search_api_filters_track_rewrite_and_rebuild() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "filter-life");
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string first =
      "# Filter Life A\n"
      "#chem\n"
      "ExpandedFilterLifeToken first\n";
  const std::string second =
      "# Filter Life B\n"
      "#chem\n"
      "ExpandedFilterLifeToken second\n";
  expect_ok(kernel_write_note(
      handle,
      "filter-life/a.md",
      first.data(),
      first.size(),
      nullptr,
      &metadata,
      &disposition));
  kernel_note_metadata second_metadata{};
  expect_ok(kernel_write_note(
      handle,
      "filter-life/b.md",
      second.data(),
      second.size(),
      nullptr,
      &second_metadata,
      &disposition));

  kernel_search_query request = make_default_search_query("ExpandedFilterLifeToken", 10);
  request.tag_filter = "chem";
  request.path_prefix = "filter-life/";
  kernel_search_page page{};
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 2, "filtered lifecycle setup should return both tagged notes");
  require_true(page.total_hits == 2, "filtered lifecycle setup should report two exact hits");
  kernel_free_search_page(&page);

  const std::string rewritten =
      "# Filter Life B\n"
      "ExpandedFilterLifeToken second without tag\n";
  expect_ok(kernel_write_note(
      handle,
      "filter-life/b.md",
      rewritten.data(),
      rewritten.size(),
      second_metadata.content_revision,
      &metadata,
      &disposition));

  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 1, "filtered search should drop notes that lose the tag on rewrite");
  require_true(page.total_hits == 1, "filtered search should keep the exact hit count after rewrite");
  require_true(std::string(page.hits[0].rel_path) == "filter-life/a.md", "filtered search should keep the remaining tagged note");
  kernel_free_search_page(&page);

  {
    std::lock_guard lock(handle->storage_mutex);
    exec_sql(
        handle->storage.connection,
        "DELETE FROM note_tags "
        "WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='filter-life/a.md');");
  }

  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 0, "manual tag drift should empty the filtered result before rebuild");
  require_true(page.total_hits == 0, "manual tag drift should drop the filtered hit count before rebuild");
  kernel_free_search_page(&page);

  require_index_ready(handle, "filtered lifecycle rebuild test should wait for READY before rebuilding");
  expect_ok(kernel_rebuild_index(handle));

  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 1, "rebuild should restore filtered search results after derived tag drift");
  require_true(page.total_hits == 1, "rebuild should restore the exact filtered hit count after drift");
  require_true(std::string(page.hits[0].rel_path) == "filter-life/a.md", "rebuild should restore the surviving tagged note");
  kernel_free_search_page(&page);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_expanded_search_api_supports_note_ranking_v1_title_boost() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string body_only =
      "# Generic Body Rank\n"
      "ExpandedRankToken appears only in the body\n";
  const std::string title_hit =
      "# ExpandedRankToken\n"
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

  kernel_search_query request = make_default_search_query("ExpandedRankToken", 10);
  request.sort_mode = KERNEL_SEARCH_SORT_RANK_V1;
  kernel_search_page page{};
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 2, "expanded Ranking v1 note search should return both matching notes");
  require_true(page.total_hits == 2, "expanded Ranking v1 note search should preserve the exact total hit count");
  require_true(std::string(page.hits[0].rel_path) == "z-title-rank.md", "expanded Ranking v1 note search should boost title hits ahead of body-only hits");
  require_true(std::string(page.hits[1].rel_path) == "a-body-rank.md", "expanded Ranking v1 note search should keep the body-only hit behind the title hit");
  require_true(
      (page.hits[0].match_flags & KERNEL_SEARCH_MATCH_TITLE) != 0,
      "expanded Ranking v1 note search should keep title match flags on the boosted title hit");
  require_true(
      page.hits[1].match_flags == KERNEL_SEARCH_MATCH_BODY,
      "expanded Ranking v1 note search should keep body-only match flags on the trailing body hit");
  kernel_free_search_page(&page);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_expanded_search_api_supports_note_ranking_v1_single_token_tag_boost() {
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

  kernel_search_query request = make_default_search_query("rankboost", 10);
  request.sort_mode = KERNEL_SEARCH_SORT_RANK_V1;
  kernel_search_page page{};
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 2, "expanded Ranking v1 tag-boost search should return both matching notes");
  require_true(std::string(page.hits[0].rel_path) == "b-tagged-rank.md", "expanded Ranking v1 tag-boost search should boost exact single-token tag matches ahead of plain body matches");
  require_true(std::string(page.hits[1].rel_path) == "a-untagged-rank.md", "expanded Ranking v1 tag-boost search should leave the untagged note behind the boosted note");
  kernel_free_search_page(&page);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_expanded_search_api_supports_kind_all_ranking_on_note_branch_only() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "rankall");
  write_file_bytes(vault / "rankall" / "expandedallranktoken-00.png", "png-00");
  write_file_bytes(vault / "rankall" / "expandedallranktoken-01.png", "png-01");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string body_note =
      "# Generic Rank All\n"
      "ExpandedAllRankToken body match only\n"
      "![Figure](rankall/expandedallranktoken-00.png)\n";
  const std::string title_note =
      "# ExpandedAllRankToken\n"
      "body text without the unique rank token\n"
      "![Figure](rankall/expandedallranktoken-01.png)\n";
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

  kernel_search_query request = make_default_search_query("ExpandedAllRankToken", 10);
  request.kind = KERNEL_SEARCH_KIND_ALL;
  request.path_prefix = "rankall/";
  request.sort_mode = KERNEL_SEARCH_SORT_RANK_V1;
  kernel_search_page page{};
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 4, "expanded Ranking v1 all-kind search should return both notes and both attachments");
  require_true(page.total_hits == 4, "expanded Ranking v1 all-kind search should report the exact combined hit count");
  require_true(std::string(page.hits[0].rel_path) == "rankall/z-title-note.md", "expanded Ranking v1 all-kind search should rank the note branch before attachments");
  require_true(std::string(page.hits[1].rel_path) == "rankall/a-body-note.md", "expanded Ranking v1 all-kind search should keep lower-ranked notes before attachments");
  require_true(std::string(page.hits[2].rel_path) == "rankall/expandedallranktoken-00.png", "expanded Ranking v1 all-kind search should append attachments after ranked notes");
  require_true(std::string(page.hits[3].rel_path) == "rankall/expandedallranktoken-01.png", "expanded Ranking v1 all-kind search should preserve attachment rel_path order");
  kernel_free_search_page(&page);

  request.limit = 2;
  request.offset = 1;
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 2, "expanded Ranking v1 all-kind pagination should return a cross-boundary page");
  require_true(std::string(page.hits[0].rel_path) == "rankall/a-body-note.md", "expanded Ranking v1 all-kind pagination should continue from the ranked note branch");
  require_true(std::string(page.hits[1].rel_path) == "rankall/expandedallranktoken-00.png", "expanded Ranking v1 all-kind pagination should enter the attachment branch after notes");
  kernel_free_search_page(&page);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_expanded_search_api_ranking_tracks_rewrite_and_rebuild() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string title_hit =
      "# ExpandedRankLifecycleToken\n"
      "body text without the unique rank token\n";
  const std::string body_only =
      "# Generic Rank Lifecycle\n"
      "ExpandedRankLifecycleToken body only\n";
  expect_ok(kernel_write_note(
      handle,
      "z-rank-lifecycle-title.md",
      title_hit.data(),
      title_hit.size(),
      nullptr,
      &metadata,
      &disposition));
  kernel_note_metadata body_metadata{};
  expect_ok(kernel_write_note(
      handle,
      "a-rank-lifecycle-body.md",
      body_only.data(),
      body_only.size(),
      nullptr,
      &body_metadata,
      &disposition));

  kernel_search_query request = make_default_search_query("ExpandedRankLifecycleToken", 10);
  request.sort_mode = KERNEL_SEARCH_SORT_RANK_V1;
  kernel_search_page page{};
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 2, "ranking lifecycle setup should return both matching notes");
  require_true(std::string(page.hits[0].rel_path) == "z-rank-lifecycle-title.md", "ranking lifecycle setup should start with the title-boosted note");
  kernel_free_search_page(&page);

  kernel_owned_buffer existing_note{};
  kernel_note_metadata existing_metadata{};
  expect_ok(kernel_read_note(handle, "z-rank-lifecycle-title.md", &existing_note, &existing_metadata));
  kernel_free_buffer(&existing_note);

  const std::string rewritten =
      "# Generic Rank Lifecycle\n"
      "body text without the rank token\n";
  expect_ok(kernel_write_note(
      handle,
      "z-rank-lifecycle-title.md",
      rewritten.data(),
      rewritten.size(),
      existing_metadata.content_revision,
      &metadata,
      &disposition));

  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 1, "ranking lifecycle should drop the removed title hit after rewrite");
  require_true(page.total_hits == 1, "ranking lifecycle should keep the exact hit count after rewrite");
  require_true(std::string(page.hits[0].rel_path) == "a-rank-lifecycle-body.md", "ranking lifecycle should leave the surviving body hit first after rewrite");
  kernel_free_search_page(&page);

  {
    std::lock_guard lock(handle->storage_mutex);
    exec_sql(
        handle->storage.connection,
        "DELETE FROM note_fts WHERE rowid=(SELECT note_id FROM notes WHERE rel_path='a-rank-lifecycle-body.md');");
  }

  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 0, "ranking lifecycle should observe missing FTS rows before rebuild repairs them");
  require_true(page.total_hits == 0, "ranking lifecycle should observe zero exact hits before rebuild repairs drift");
  kernel_free_search_page(&page);

  require_index_ready(handle, "ranking lifecycle rebuild test should wait for READY before rebuilding");
  expect_ok(kernel_rebuild_index(handle));

  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 1, "ranking lifecycle should restore the surviving hit after rebuild");
  require_true(page.total_hits == 1, "ranking lifecycle should restore the exact hit count after rebuild");
  require_true(std::string(page.hits[0].rel_path) == "a-rank-lifecycle-body.md", "ranking lifecycle should restore the surviving body hit after rebuild");
  kernel_free_search_page(&page);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}


}  // namespace

void run_search_expanded_tests() {
  test_expanded_search_api_returns_body_snippet_and_exact_total_hits();
  test_expanded_search_api_returns_title_only_without_snippet();
  test_expanded_search_api_strips_title_heading_and_collapses_body_whitespace();
  test_expanded_search_api_supports_note_tag_and_path_prefix_filters();
  test_expanded_search_api_supports_exact_offset_limit_pagination();
  test_expanded_search_api_rejects_invalid_page_limits();
  test_expanded_search_api_pagination_tracks_rewrite_and_rebuild();
  test_expanded_search_api_supports_attachment_path_hits_and_missing_flag();
  test_expanded_search_api_supports_kind_all_notes_first_then_attachments();
  test_expanded_search_api_rejects_invalid_filter_and_ranking_combinations_and_clears_stale_output();
  test_expanded_search_api_filters_track_rewrite_and_rebuild();
  test_expanded_search_api_supports_note_ranking_v1_title_boost();
  test_expanded_search_api_supports_note_ranking_v1_single_token_tag_boost();
  test_expanded_search_api_supports_kind_all_ranking_on_note_branch_only();
  test_expanded_search_api_ranking_tracks_rewrite_and_rebuild();
}
