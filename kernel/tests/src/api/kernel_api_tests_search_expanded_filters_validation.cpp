// Reason: This file isolates expanded search filter validation and lifecycle coverage so basic filter surface tests stay smaller.

#include "kernel/c_api.h"

#include "api/kernel_api_search_expanded_filter_suites.h"
#include "api/kernel_api_search_public_surface_support.h"
#include "api/kernel_api_test_support.h"
#include "core/kernel_internal.h"
#include "support/test_support.h"

#include <filesystem>
#include <mutex>
#include <string>

namespace {

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

}  // namespace

void run_search_expanded_filter_validation_tests() {
  test_expanded_search_api_rejects_invalid_filter_and_ranking_combinations_and_clears_stale_output();
  test_expanded_search_api_filters_track_rewrite_and_rebuild();
}
