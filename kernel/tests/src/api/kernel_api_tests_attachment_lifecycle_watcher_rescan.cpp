// Reason: This file isolates attachment watcher rename and full-rescan coverage so lifecycle regressions stay easy to navigate.

#include "kernel/c_api.h"

#include "api/kernel_api_attachment_lifecycle_watcher_suites.h"
#include "api/kernel_api_test_support.h"
#include "storage/storage.h"
#include "support/test_support.h"
#include "watcher/integration.h"

#include <filesystem>
#include <string>
#include <vector>

namespace {

void test_attachment_api_observes_attachment_rename_reconciliation() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "assets");
  write_file_bytes(vault / "assets" / "old.bin", "old-attachment");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string content =
      "# Attachment Rename\n"
      "[Old](assets/old.bin)\n";
  expect_ok(kernel_write_note(
      handle,
      "attachment-rename.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_close(handle));

  auto db = kernel::storage::Database{};
  require_true(
      !kernel::storage::open_or_create(storage_db_for_vault(vault), db),
      "attachment rename API test should open storage db");
  require_true(
      !kernel::storage::ensure_schema_v1(db),
      "attachment rename API test should ensure schema");

  std::filesystem::rename(vault / "assets" / "old.bin", vault / "assets" / "renamed.bin");
  const std::vector<kernel::watcher::CoalescedAction> actions = {
      {kernel::watcher::CoalescedActionKind::RenamePath, "assets/old.bin", "assets/renamed.bin"}};
  require_true(!kernel::watcher::apply_actions(db, vault, actions), "attachment rename apply should succeed");
  kernel::storage::close(db);

  handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "attachment rename API test should settle to READY");

  kernel_attachment_metadata attachment{};
  expect_ok(kernel_get_attachment_metadata(handle, "assets/old.bin", &attachment));
  require_true(attachment.is_missing == 1, "attachment rename should expose the old path as missing");
  expect_ok(kernel_get_attachment_metadata(handle, "assets/renamed.bin", &attachment));
  require_true(attachment.is_missing == 0, "attachment rename should expose the new path as present");

  kernel_attachment_list attachments{};
  expect_ok(kernel_query_attachments(handle, static_cast<size_t>(-1), &attachments));
  require_true(
      attachments.count == 1,
      "attachment rename should keep only the still-referenced path in the formal live catalog");
  require_true(
      std::string(attachments.attachments[0].rel_path) == "assets/old.bin" &&
          attachments.attachments[0].presence == KERNEL_ATTACHMENT_PRESENCE_MISSING,
      "attachment rename should keep the old live attachment path visible as missing in the formal catalog");
  kernel_free_attachment_list(&attachments);

  require_attachment_lookup_state(
      handle,
      "assets/old.bin",
      KERNEL_ATTACHMENT_PRESENCE_MISSING,
      1,
      KERNEL_ATTACHMENT_KIND_GENERIC_FILE,
      true,
      "attachment rename should keep the old ref path visible in the formal single-attachment surface");
  require_single_note_attachment_ref_state(
      handle,
      "attachment-rename.md",
      "assets/old.bin",
      KERNEL_ATTACHMENT_PRESENCE_MISSING,
      1,
      KERNEL_ATTACHMENT_KIND_GENERIC_FILE,
      true,
      "attachment rename should keep note attachment refs pinned to the old live path until note rewrite");
  require_attachment_lookup_not_found(
      handle,
      "assets/renamed.bin",
      "attachment rename should exclude the unreferenced renamed path from the formal single-attachment surface");

  kernel_attachment_referrers referrers{};
  expect_ok(kernel_query_attachment_referrers(
      handle,
      "assets/old.bin",
      static_cast<size_t>(-1),
      &referrers));
  require_true(
      referrers.count == 1,
      "attachment rename should keep the old live path's formal referrers visible");
  require_true(
      std::string(referrers.referrers[0].note_rel_path) == "attachment-rename.md",
      "attachment rename should keep referrers pinned to the old live path");
  kernel_free_attachment_referrers(&referrers);
  require_attachment_referrers_not_found(
      handle,
      "assets/renamed.bin",
      "attachment rename should exclude the unreferenced renamed path from the formal referrers surface");

  kernel_search_query request{};
  request.query = "old";
  request.limit = 8;
  request.kind = KERNEL_SEARCH_KIND_ATTACHMENT;
  request.sort_mode = KERNEL_SEARCH_SORT_REL_PATH_ASC;
  kernel_search_page page{};
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(
      page.count == 1 && page.total_hits == 1,
      "attachment rename should keep the old live path searchable through the attachment path surface");
  require_true(
      std::string(page.hits[0].rel_path) == "assets/old.bin" &&
          page.hits[0].result_flags == KERNEL_SEARCH_RESULT_FLAG_ATTACHMENT_MISSING,
      "attachment rename search should expose the old live path as missing");
  kernel_free_search_page(&page);

  request.query = "renamed";
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(
      page.count == 0 && page.total_hits == 0,
      "attachment rename should exclude the unreferenced renamed path from attachment path search");
  kernel_free_search_page(&page);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_attachment_full_rescan_reconciles_mixed_changes_through_formal_public_surface() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "assets");
  std::filesystem::create_directories(vault / "docs");
  write_file_bytes(vault / "assets" / "live.png", "live-attachment");
  write_file_bytes(vault / "assets" / "remove.bin", "remove-attachment");
  write_file_bytes(vault / "assets" / "stale.png", "stale-attachment");

  kernel_handle* handle = nullptr;
  require_true(
      kernel_open_vault(vault.string().c_str(), &handle).code == KERNEL_OK,
      "attachment full-rescan test should open vault");

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string before =
      "# Attachment Full Rescan Before\n"
      "![Live](assets/live.png)\n"
      "[Remove](assets/remove.bin)\n"
      "![Stale](assets/stale.png)\n";
  require_true(
      kernel_write_note(
          handle,
          "attachment-full-rescan.md",
          before.data(),
          before.size(),
          nullptr,
          &metadata,
          &disposition)
              .code == KERNEL_OK,
      "attachment full-rescan test should seed the initial note");
  require_true(
      kernel_close(handle).code == KERNEL_OK,
      "attachment full-rescan test should close the seeded vault");

  std::filesystem::remove(vault / "assets" / "stale.png");
  write_file_bytes(vault / "docs" / "fresh.pdf", "fresh-pdf");
  const std::string after =
      "# Attachment Full Rescan After\n"
      "![Live](assets/live.png)\n"
      "![Stale](assets/stale.png)\n"
      "[Fresh](docs/fresh.pdf)\n";
  write_file_bytes(vault / "attachment-full-rescan.md", after);

  auto db = kernel::storage::Database{};
  require_true(
      !kernel::storage::open_or_create(storage_db_for_vault(vault), db),
      "attachment full-rescan test should open storage db");
  require_true(
      !kernel::storage::ensure_schema_v1(db),
      "attachment full-rescan test should ensure schema");
  const std::vector<kernel::watcher::CoalescedAction> actions = {
      {kernel::watcher::CoalescedActionKind::FullRescan, ""}};
  require_true(
      !kernel::watcher::apply_actions(db, vault, actions),
      "attachment full-rescan apply should succeed");
  kernel::storage::close(db);

  handle = nullptr;
  require_true(
      kernel_open_vault(vault.string().c_str(), &handle).code == KERNEL_OK,
      "attachment full-rescan test should reopen vault after offline full rescan");
  require_index_ready(handle, "attachment full-rescan test should settle to READY");

  kernel_attachment_list refs{};
  require_true(
      kernel_query_note_attachment_refs(
          handle,
          "attachment-full-rescan.md",
          static_cast<size_t>(-1),
          &refs)
              .code == KERNEL_OK,
      "attachment full-rescan should expose refreshed note attachment refs");
  require_true(
      refs.count == 3,
      "attachment full-rescan should replace the live note attachment ref set in the formal public surface");
  require_true(
      std::string(refs.attachments[0].rel_path) == "assets/live.png" &&
          refs.attachments[0].presence == KERNEL_ATTACHMENT_PRESENCE_PRESENT &&
          refs.attachments[0].kind == KERNEL_ATTACHMENT_KIND_IMAGE_LIKE,
      "attachment full-rescan should preserve the still-live present attachment ref");
  require_true(
      std::string(refs.attachments[1].rel_path) == "assets/stale.png" &&
          refs.attachments[1].presence == KERNEL_ATTACHMENT_PRESENCE_MISSING &&
          refs.attachments[1].kind == KERNEL_ATTACHMENT_KIND_IMAGE_LIKE,
      "attachment full-rescan should keep deleted-but-still-referenced attachments visible as missing");
  require_true(
      std::string(refs.attachments[2].rel_path) == "docs/fresh.pdf" &&
          refs.attachments[2].presence == KERNEL_ATTACHMENT_PRESENCE_PRESENT &&
          refs.attachments[2].kind == KERNEL_ATTACHMENT_KIND_PDF_LIKE,
      "attachment full-rescan should add newly referenced attachments to the formal note refs surface");
  kernel_free_attachment_list(&refs);

  kernel_attachment_list attachments{};
  require_true(
      kernel_query_attachments(handle, static_cast<size_t>(-1), &attachments).code == KERNEL_OK,
      "attachment full-rescan should expose the refreshed live catalog");
  require_true(
      attachments.count == 3,
      "attachment full-rescan should expose only the refreshed live catalog through the formal list surface");
  require_true(
      std::string(attachments.attachments[0].rel_path) == "assets/live.png" &&
          attachments.attachments[0].presence == KERNEL_ATTACHMENT_PRESENCE_PRESENT,
      "attachment full-rescan catalog should keep present live paths");
  require_true(
      std::string(attachments.attachments[1].rel_path) == "assets/stale.png" &&
          attachments.attachments[1].presence == KERNEL_ATTACHMENT_PRESENCE_MISSING,
      "attachment full-rescan catalog should keep missing live paths");
  require_true(
      std::string(attachments.attachments[2].rel_path) == "docs/fresh.pdf" &&
          attachments.attachments[2].presence == KERNEL_ATTACHMENT_PRESENCE_PRESENT,
      "attachment full-rescan catalog should add new live paths");
  kernel_free_attachment_list(&attachments);

  require_attachment_lookup_state(
      handle,
      "assets/live.png",
      KERNEL_ATTACHMENT_PRESENCE_PRESENT,
      1,
      KERNEL_ATTACHMENT_KIND_IMAGE_LIKE,
      true,
      "attachment full-rescan should preserve present attachment lookup state");
  require_attachment_lookup_state(
      handle,
      "assets/stale.png",
      KERNEL_ATTACHMENT_PRESENCE_MISSING,
      1,
      KERNEL_ATTACHMENT_KIND_IMAGE_LIKE,
      true,
      "attachment full-rescan should reconcile deleted attachment lookup state to missing");
  require_attachment_lookup_state(
      handle,
      "docs/fresh.pdf",
      KERNEL_ATTACHMENT_PRESENCE_PRESENT,
      1,
      KERNEL_ATTACHMENT_KIND_PDF_LIKE,
      true,
      "attachment full-rescan should expose newly referenced attachment lookup state");
  require_attachment_lookup_not_found(
      handle,
      "assets/remove.bin",
      "attachment full-rescan should drop removed refs from the formal live catalog");
  require_attachment_referrers_not_found(
      handle,
      "assets/remove.bin",
      "attachment full-rescan should drop removed refs from the formal referrers surface");

  kernel_search_query request{};
  request.limit = 8;
  request.kind = KERNEL_SEARCH_KIND_ATTACHMENT;
  request.sort_mode = KERNEL_SEARCH_SORT_REL_PATH_ASC;

  kernel_search_page page{};
  request.query = "fresh";
  require_true(
      kernel_query_search(handle, &request, &page).code == KERNEL_OK,
      "attachment full-rescan search should query fresh attachment paths");
  require_true(
      page.count == 1 && page.total_hits == 1 &&
          std::string(page.hits[0].rel_path) == "docs/fresh.pdf" &&
          page.hits[0].result_flags == KERNEL_SEARCH_RESULT_FLAG_NONE,
      "attachment full-rescan search should expose newly referenced live attachment paths");
  kernel_free_search_page(&page);

  request.query = "stale";
  require_true(
      kernel_query_search(handle, &request, &page).code == KERNEL_OK,
      "attachment full-rescan search should query missing live attachment paths");
  require_true(
      page.count == 1 && page.total_hits == 1 &&
          std::string(page.hits[0].rel_path) == "assets/stale.png" &&
          page.hits[0].result_flags == KERNEL_SEARCH_RESULT_FLAG_ATTACHMENT_MISSING,
      "attachment full-rescan search should keep deleted-but-live attachment paths marked as missing");
  kernel_free_search_page(&page);

  request.query = "remove";
  require_true(
      kernel_query_search(handle, &request, &page).code == KERNEL_OK,
      "attachment full-rescan search should query removed attachment paths");
  require_true(
      page.count == 0 && page.total_hits == 0,
      "attachment full-rescan search should exclude paths that are no longer in the live catalog");
  kernel_free_search_page(&page);

  require_true(
      kernel_close(handle).code == KERNEL_OK,
      "attachment full-rescan test should close the reopened vault");
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_attachment_lifecycle_watcher_rescan_tests() {
  test_attachment_api_observes_attachment_rename_reconciliation();
  test_attachment_full_rescan_reconciles_mixed_changes_through_formal_public_surface();
}
