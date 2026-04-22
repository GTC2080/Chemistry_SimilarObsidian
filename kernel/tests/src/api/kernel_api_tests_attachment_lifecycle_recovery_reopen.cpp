// Reason: This file isolates attachment reopen catch-up repair coverage so startup recovery scenarios can stay smaller.

#include "kernel/c_api.h"

#include "api/kernel_api_attachment_lifecycle_recovery_suites.h"
#include "api/kernel_api_test_support.h"
#include "support/test_support.h"
#include "third_party/sqlite/sqlite3.h"

#include <filesystem>
#include <string>

namespace {

void test_reopen_catch_up_repairs_attachment_missing_state_after_closed_window_delete() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);
  std::filesystem::create_directories(vault / "assets");
  write_file_bytes(vault / "assets" / "closed-window-delete.png", "attachment-bytes");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Closed Window Attachment\n"
      "![Figure](assets/closed-window-delete.png)\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "closed-window-attachment.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));
  require_index_ready(handle, "attachment closed-window delete test should start from READY");
  expect_ok(kernel_close(handle));

  std::filesystem::remove(vault / "assets" / "closed-window-delete.png");

  handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "reopen after closed-window attachment delete should settle to READY");

  sqlite3* db = open_sqlite_readonly(db_path);
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM attachments "
          "WHERE rel_path='assets/closed-window-delete.png' AND is_missing=1;") == 1,
      "reopen catch-up should mark a closed-window attachment delete as missing");
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM note_attachment_refs "
          "WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='closed-window-attachment.md') "
          "AND attachment_rel_path='assets/closed-window-delete.png';") == 1,
      "reopen catch-up should preserve note attachment refs while reconciling missing attachment state");
  sqlite3_close(db);

  require_single_note_attachment_ref_state(
      handle,
      "closed-window-attachment.md",
      "assets/closed-window-delete.png",
      KERNEL_ATTACHMENT_PRESENCE_MISSING,
      1,
      KERNEL_ATTACHMENT_KIND_IMAGE_LIKE,
      true,
      "reopen catch-up should preserve the live attachment ref through the formal public surface");
  require_attachment_lookup_state(
      handle,
      "assets/closed-window-delete.png",
      KERNEL_ATTACHMENT_PRESENCE_MISSING,
      1,
      KERNEL_ATTACHMENT_KIND_IMAGE_LIKE,
      true,
      "reopen catch-up should expose missing attachment metadata through the formal public surface");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_attachment_lifecycle_recovery_reopen_tests() {
  test_reopen_catch_up_repairs_attachment_missing_state_after_closed_window_delete();
}
