// Reason: Keep attachment ref/referrer and orphan-filtering coverage separate from catalog surface checks.

#include "kernel/c_api.h"

#include "api/kernel_api_attachment_public_surface_suites.h"
#include "api/kernel_api_test_support.h"
#include "support/test_support.h"

#include <filesystem>
#include <string>

namespace {

void test_attachment_public_surface_note_refs_and_referrers_are_stable() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "assets");
  std::filesystem::create_directories(vault / "docs");
  write_file_bytes(vault / "assets" / "diagram.png", "diagram-bytes");
  write_file_bytes(vault / "docs" / "paper.pdf", "paper-bytes");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string alpha =
      "# Alpha Attachment Note\n"
      "[Paper](docs/paper.pdf)\n"
      "![Diagram](assets/diagram.png)\n";
  expect_ok(kernel_write_note(
      handle,
      "alpha-attachments.md",
      alpha.data(),
      alpha.size(),
      nullptr,
      &metadata,
      &disposition));

  const std::string beta =
      "# Beta Attachment Note\n"
      "[Paper](docs/paper.pdf)\n";
  expect_ok(kernel_write_note(
      handle,
      "beta-attachments.md",
      beta.data(),
      beta.size(),
      nullptr,
      &metadata,
      &disposition));
  const std::string gamma =
      "# Gamma Attachment Note\n"
      "plain body without attachment refs\n";
  expect_ok(kernel_write_note(
      handle,
      "gamma-attachments.md",
      gamma.data(),
      gamma.size(),
      nullptr,
      &metadata,
      &disposition));

  kernel_attachment_list refs{};
  expect_ok(kernel_query_note_attachment_refs(
      handle,
      "alpha-attachments.md",
      static_cast<size_t>(-1),
      &refs));
  require_true(refs.count == 2, "note attachment refs surface should return both live attachments");
  require_true(
      std::string(refs.attachments[0].rel_path) == "docs/paper.pdf",
      "note attachment refs surface should preserve parser order");
  require_true(
      std::string(refs.attachments[1].rel_path) == "assets/diagram.png",
      "note attachment refs surface should preserve parser order across multiple refs");
  require_true(
      refs.attachments[0].ref_count == 2,
      "note attachment refs surface should expose global live ref counts");
  require_true(
      refs.attachments[1].ref_count == 1,
      "note attachment refs surface should expose per-attachment live ref counts");
  kernel_free_attachment_list(&refs);

  expect_ok(kernel_query_note_attachment_refs(
      handle,
      "gamma-attachments.md",
      static_cast<size_t>(-1),
      &refs));
  require_true(
      refs.count == 0,
      "note attachment refs surface should succeed with an empty result for live notes without attachment refs");
  kernel_free_attachment_list(&refs);

  kernel_attachment_referrers referrers{};
  expect_ok(kernel_query_attachment_referrers(
      handle,
      "docs/paper.pdf",
      static_cast<size_t>(-1),
      &referrers));
  require_true(
      referrers.count == 2,
      "attachment referrers surface should report every live note that references the attachment");
  require_true(
      std::string(referrers.referrers[0].note_rel_path) == "alpha-attachments.md",
      "attachment referrers surface should sort by note rel_path");
  require_true(
      std::string(referrers.referrers[1].note_rel_path) == "beta-attachments.md",
      "attachment referrers surface should keep note rel_path ordering stable");
  require_true(
      std::string(referrers.referrers[0].note_title) == "Alpha Attachment Note",
      "attachment referrers surface should expose note titles");
  require_true(
      std::string(referrers.referrers[1].note_title) == "Beta Attachment Note",
      "attachment referrers surface should expose note titles");
  kernel_free_attachment_referrers(&referrers);

  require_true(
      kernel_query_attachments(handle, 0, nullptr).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "attachment catalog surface should reject null output and zero limit");
  require_true(
      kernel_query_note_attachment_refs(handle, "", static_cast<size_t>(-1), &refs).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "note attachment refs surface should reject empty note paths");
  require_true(
      kernel_query_attachment_referrers(handle, "..\\paper.pdf", static_cast<size_t>(-1), &referrers)
              .code == KERNEL_ERROR_INVALID_ARGUMENT,
      "attachment referrers surface should reject path traversal");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_attachment_public_surface_excludes_orphaned_paths_and_matches_search() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);
  std::filesystem::create_directories(vault / "assets");
  std::filesystem::create_directories(vault / "docs");
  write_file_bytes(vault / "assets" / "stale.png", "stale-bytes");
  write_file_bytes(vault / "docs" / "live.pdf", "live-bytes");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string original =
      "# Orphan Attachment Note\n"
      "![Stale](assets/stale.png)\n";
  expect_ok(kernel_write_note(
      handle,
      "orphan-attachments.md",
      original.data(),
      original.size(),
      nullptr,
      &metadata,
      &disposition));

  const std::string rewritten =
      "# Orphan Attachment Note\n"
      "[Live](docs/live.pdf)\n";
  expect_ok(kernel_write_note(
      handle,
      "orphan-attachments.md",
      rewritten.data(),
      rewritten.size(),
      metadata.content_revision,
      &metadata,
      &disposition));

  sqlite3* db = open_sqlite_readonly(db_path);
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM attachments WHERE rel_path='assets/stale.png';") == 1,
      "rewrite should leave the stale attachment metadata row behind for orphan filtering coverage");
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM note_attachment_refs "
          "WHERE attachment_rel_path='assets/stale.png';") == 0,
      "rewrite should clear stale note attachment refs");
  sqlite3_close(db);

  kernel_attachment_list attachments{};
  expect_ok(kernel_query_attachments(handle, static_cast<size_t>(-1), &attachments));
  require_true(attachments.count == 1, "attachment catalog surface should exclude orphaned paths");
  require_true(
      std::string(attachments.attachments[0].rel_path) == "docs/live.pdf",
      "attachment catalog surface should only keep live paths after rewrite");
  kernel_free_attachment_list(&attachments);

  kernel_attachment_record attachment{};
  require_true(
      kernel_get_attachment(handle, "assets/stale.png", &attachment).code == KERNEL_ERROR_NOT_FOUND,
      "single attachment lookup should exclude orphaned metadata rows");
  kernel_attachment_referrers referrers{};
  require_true(
      kernel_query_attachment_referrers(
          handle,
          "assets/stale.png",
          static_cast<size_t>(-1),
          &referrers)
              .code == KERNEL_ERROR_NOT_FOUND,
      "attachment referrers surface should exclude orphaned metadata rows");

  kernel_search_query request{};
  request.query = "stale";
  request.limit = 8;
  request.kind = KERNEL_SEARCH_KIND_ATTACHMENT;
  request.sort_mode = KERNEL_SEARCH_SORT_REL_PATH_ASC;
  kernel_search_page page{};
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 0 && page.total_hits == 0, "attachment path search should exclude orphaned paths");
  kernel_free_search_page(&page);

  request.query = "live";
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 1 && page.total_hits == 1, "attachment path search should still expose the live attachment path");
  require_true(
      std::string(page.hits[0].rel_path) == "docs/live.pdf",
      "attachment path search should agree with the public attachment surface");
  kernel_free_search_page(&page);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_attachment_public_surface_refs_tests() {
  test_attachment_public_surface_note_refs_and_referrers_are_stable();
  test_attachment_public_surface_excludes_orphaned_paths_and_matches_search();
}
