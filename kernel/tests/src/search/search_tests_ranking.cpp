// Reason: Isolate ranking-v1 regressions so ranking-specific expectations stay out of the base search suites.

#include "search/search_test_suites.h"

#include "kernel/c_api.h"
#include "search/search.h"
#include "search/search_test_support.h"
#include "support/test_support.h"

#include <filesystem>
#include <string>
#include <system_error>

namespace {

using search_tests::open_search_db;

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

}  // namespace

void run_search_ranking_tests() {
  test_search_page_ranking_boosts_title_hits_before_body_only_hits();
  test_search_page_ranking_boosts_single_token_exact_tag_hits();
  test_search_page_ranking_tie_breaks_by_rel_path();
  test_search_page_ranking_all_kind_ranks_note_branch_and_appends_attachments();
  test_search_page_ranking_rejects_attachment_sort_mode();
}
