// Reason: This file isolates watcher backoff attachment regressions so the broader lifecycle suite can stay focused on composition.

#include "kernel/c_api.h"

#include "api/kernel_api_attachment_lifecycle_watcher_suites.h"
#include "api/kernel_api_test_support.h"
#include "core/kernel_internal.h"
#include "support/test_support.h"
#include "third_party/sqlite/sqlite3.h"
#include "watcher/session.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>

namespace {

void test_close_during_watcher_fault_backoff_leaves_attachment_create_for_reopen_catch_up() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);
  std::filesystem::create_directories(vault / "assets");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Backoff Attachment Create\n"
      "![Figure](assets/backoff-created.png)\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "backoff-attachment-create.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));
  require_index_ready(handle, "watcher backoff attachment-create test should start from READY");

  sqlite3* db = open_sqlite_readonly(db_path);
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM attachments "
          "WHERE rel_path='assets/backoff-created.png' AND is_missing=1;") == 1,
      "seed write should register the missing attachment before the closed-window create");
  sqlite3_close(db);

  kernel::watcher::inject_next_poll_errors(handle->watcher_session, std::errc::io_error, 20);
  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_UNAVAILABLE;
      },
      "watcher should enter UNAVAILABLE backoff before closed-window attachment create");

  write_file_bytes(vault / "assets" / "backoff-created.png", "attachment-bytes");
  expect_ok(kernel_close(handle));

  db = open_sqlite_readonly(db_path);
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM attachments "
          "WHERE rel_path='assets/backoff-created.png' AND is_missing=1;") == 1,
      "close during watcher backoff should leave the stale missing attachment row for reopen catch-up to reconcile");
  sqlite3_close(db);

  handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "reopen after watcher-backoff attachment create should settle to READY");

  db = open_sqlite_readonly(db_path);
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM attachments "
          "WHERE rel_path='assets/backoff-created.png' AND is_missing=0;") == 1,
      "reopen catch-up should reconcile a closed-window attachment create back to present state");
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM note_attachment_refs "
          "WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='backoff-attachment-create.md') "
          "AND attachment_rel_path='assets/backoff-created.png';") == 1,
      "reopen catch-up should preserve note attachment refs while reconciling attachment create");
  sqlite3_close(db);

  require_single_note_attachment_ref_state(
      handle,
      "backoff-attachment-create.md",
      "assets/backoff-created.png",
      KERNEL_ATTACHMENT_PRESENCE_PRESENT,
      1,
      KERNEL_ATTACHMENT_KIND_IMAGE_LIKE,
      true,
      "reopen catch-up should expose closed-window attachment create through the formal public surface");
  require_attachment_lookup_state(
      handle,
      "assets/backoff-created.png",
      KERNEL_ATTACHMENT_PRESENCE_PRESENT,
      1,
      KERNEL_ATTACHMENT_KIND_IMAGE_LIKE,
      true,
      "reopen catch-up should expose created attachment metadata through the formal public surface");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_close_during_watcher_fault_backoff_leaves_attachment_delete_for_reopen_catch_up() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);
  std::filesystem::create_directories(vault / "assets");
  write_file_bytes(vault / "assets" / "backoff-delete.png", "attachment-bytes");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Backoff Attachment Delete\n"
      "![Figure](assets/backoff-delete.png)\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "backoff-attachment-delete.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));
  require_index_ready(handle, "watcher backoff attachment-delete test should start from READY");

  sqlite3* db = open_sqlite_readonly(db_path);
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM attachments "
          "WHERE rel_path='assets/backoff-delete.png' AND is_missing=0;") == 1,
      "seed write should register the present attachment before the closed-window delete");
  sqlite3_close(db);

  kernel::watcher::inject_next_poll_errors(handle->watcher_session, std::errc::io_error, 20);
  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_UNAVAILABLE;
      },
      "watcher should enter UNAVAILABLE backoff before closed-window attachment delete");

  std::filesystem::remove(vault / "assets" / "backoff-delete.png");
  expect_ok(kernel_close(handle));

  db = open_sqlite_readonly(db_path);
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM attachments "
          "WHERE rel_path='assets/backoff-delete.png' AND is_missing=0;") == 1,
      "close during watcher backoff should leave the stale present attachment row for reopen catch-up to reconcile");
  sqlite3_close(db);

  handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "reopen after watcher-backoff attachment delete should settle to READY");

  db = open_sqlite_readonly(db_path);
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM attachments "
          "WHERE rel_path='assets/backoff-delete.png' AND is_missing=1;") == 1,
      "reopen catch-up should reconcile a closed-window attachment delete back to missing state");
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM note_attachment_refs "
          "WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='backoff-attachment-delete.md') "
          "AND attachment_rel_path='assets/backoff-delete.png';") == 1,
      "reopen catch-up should preserve note attachment refs while reconciling attachment delete");
  sqlite3_close(db);

  require_single_note_attachment_ref_state(
      handle,
      "backoff-attachment-delete.md",
      "assets/backoff-delete.png",
      KERNEL_ATTACHMENT_PRESENCE_MISSING,
      1,
      KERNEL_ATTACHMENT_KIND_IMAGE_LIKE,
      true,
      "reopen catch-up should expose closed-window attachment delete through the formal public surface");
  require_attachment_lookup_state(
      handle,
      "assets/backoff-delete.png",
      KERNEL_ATTACHMENT_PRESENCE_MISSING,
      1,
      KERNEL_ATTACHMENT_KIND_IMAGE_LIKE,
      true,
      "reopen catch-up should expose deleted attachment metadata through the formal public surface");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_close_during_watcher_fault_backoff_leaves_attachment_modify_for_reopen_catch_up() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);
  std::filesystem::create_directories(vault / "assets");
  const auto attachment_path = vault / "assets" / "backoff-modify.bin";
  write_file_bytes(attachment_path, "old-bytes");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Backoff Attachment Modify\n"
      "![Figure](assets/backoff-modify.bin)\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "backoff-attachment-modify.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));
  require_index_ready(handle, "watcher backoff attachment-modify test should start from READY");

  sqlite3* db = open_sqlite_readonly(db_path);
  const auto original_size =
      query_single_int(db, "SELECT file_size FROM attachments WHERE rel_path='assets/backoff-modify.bin';");
  sqlite3_close(db);

  kernel::watcher::inject_next_poll_errors(handle->watcher_session, std::errc::io_error, 20);
  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_UNAVAILABLE;
      },
      "watcher should enter UNAVAILABLE backoff before closed-window attachment modify");

  write_file_bytes(attachment_path, "new-bytes-that-are-longer");
  expect_ok(kernel_close(handle));

  db = open_sqlite_readonly(db_path);
  require_true(
      query_single_int(db, "SELECT file_size FROM attachments WHERE rel_path='assets/backoff-modify.bin';") ==
          original_size,
      "close during watcher backoff should leave stale attachment metadata for reopen catch-up to reconcile");
  sqlite3_close(db);

  handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "reopen after watcher-backoff attachment modify should settle to READY");

  db = open_sqlite_readonly(db_path);
  require_true(
      query_single_int(db, "SELECT file_size FROM attachments WHERE rel_path='assets/backoff-modify.bin';") >
          original_size,
      "reopen catch-up should reconcile a closed-window attachment modify back to current metadata");
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM note_attachment_refs "
          "WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='backoff-attachment-modify.md') "
          "AND attachment_rel_path='assets/backoff-modify.bin';") == 1,
      "reopen catch-up should preserve note attachment refs while reconciling attachment modify");
  sqlite3_close(db);

  require_single_note_attachment_ref_state(
      handle,
      "backoff-attachment-modify.md",
      "assets/backoff-modify.bin",
      KERNEL_ATTACHMENT_PRESENCE_PRESENT,
      1,
      KERNEL_ATTACHMENT_KIND_GENERIC_FILE,
      true,
      "reopen catch-up should expose closed-window attachment modify through the formal public surface");
  {
    kernel_attachment_record attachment{};
    expect_ok(kernel_get_attachment(handle, "assets/backoff-modify.bin", &attachment));
    require_true(
        attachment.presence == KERNEL_ATTACHMENT_PRESENCE_PRESENT,
        "reopen catch-up should keep modified attachment present in the formal public surface");
    require_true(
        attachment.kind == KERNEL_ATTACHMENT_KIND_GENERIC_FILE,
        "reopen catch-up should preserve modified attachment kind in the formal public surface");
    require_true(
        attachment.ref_count == 1,
        "reopen catch-up should preserve modified attachment ref_count in the formal public surface");
    require_true(
        attachment.file_size > static_cast<std::uint64_t>(original_size),
        "reopen catch-up should refresh modified attachment file_size in the formal public surface");
    require_true(
        attachment.mtime_ns > 0,
        "reopen catch-up should refresh modified attachment mtime in the formal public surface");
    kernel_free_attachment_record(&attachment);
  }

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_attachment_lifecycle_watcher_backoff_tests() {
  test_close_during_watcher_fault_backoff_leaves_attachment_create_for_reopen_catch_up();
  test_close_during_watcher_fault_backoff_leaves_attachment_delete_for_reopen_catch_up();
  test_close_during_watcher_fault_backoff_leaves_attachment_modify_for_reopen_catch_up();
}
