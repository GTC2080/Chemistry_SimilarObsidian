// Reason: Keep expanded Ranking v1 regressions separate from snippet, pagination, and filter coverage.

#include "kernel/c_api.h"

#include "api/kernel_api_search_expanded_suites.h"
#include "api/kernel_api_search_public_surface_support.h"
#include "api/kernel_api_test_support.h"
#include "core/kernel_internal.h"
#include "support/test_support.h"

#include <filesystem>
#include <mutex>
#include <string>

namespace {

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

void run_search_expanded_ranking_tests() {
  test_expanded_search_api_supports_note_ranking_v1_title_boost();
  test_expanded_search_api_supports_note_ranking_v1_single_token_tag_boost();
  test_expanded_search_api_supports_kind_all_ranking_on_note_branch_only();
  test_expanded_search_api_ranking_tracks_rewrite_and_rebuild();
}
