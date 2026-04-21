// Reason: This file isolates expanded search filter surface coverage so validation and lifecycle checks can stay separate.

#include "kernel/c_api.h"

#include "api/kernel_api_search_expanded_filter_suites.h"
#include "api/kernel_api_search_public_surface_support.h"
#include "api/kernel_api_test_support.h"
#include "support/test_support.h"

#include <filesystem>
#include <string>

namespace {

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

}  // namespace

void run_search_expanded_filter_surface_tests() {
  test_expanded_search_api_supports_note_tag_and_path_prefix_filters();
  test_expanded_search_api_supports_attachment_path_hits_and_missing_flag();
  test_expanded_search_api_supports_kind_all_notes_first_then_attachments();
}
