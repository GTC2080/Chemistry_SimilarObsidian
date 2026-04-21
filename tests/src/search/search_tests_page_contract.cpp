// Reason: Keep paged search contract regressions together because they all verify stable result-shape and filtering semantics.

#include "search/search_snippet_pagination_suites.h"

#include "index/refresh.h"
#include "kernel/c_api.h"
#include "search/search.h"
#include "search/search_test_support.h"
#include "support/test_support.h"

#include <filesystem>
#include <string>

namespace {

using search_tests::open_search_db;

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

}  // namespace

void run_search_page_contract_tests() {
  test_search_page_supports_offset_limit_and_exact_total_hits();
  test_search_page_returns_empty_page_when_offset_is_out_of_range();
  test_search_page_filters_notes_by_tag_and_path_prefix();
  test_search_page_finds_attachment_paths_and_marks_missing();
  test_search_page_merges_all_kinds_with_notes_first_and_rel_path_order();
}
