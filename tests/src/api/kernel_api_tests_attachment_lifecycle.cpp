// Reason: This file isolates attachment lifecycle regressions so recovery, catch-up, rebuild, and watcher-backoff behavior stop bloating the main API suite.

#include "kernel/c_api.h"

#include "api/kernel_api_test_support.h"
#include "core/kernel_internal.h"
#include "recovery/journal.h"
#include "storage/storage.h"
#include "support/test_support.h"
#include "watcher/integration.h"
#include "watcher/session.h"

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <system_error>
#include <vector>

namespace {

void test_startup_recovery_replaces_stale_attachment_refs_and_metadata() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);
  const auto state_dir = state_dir_for_vault(vault);
  const auto journal_path = journal_path_for_vault(vault);
  const auto target_path = vault / "recover-attachments.md";
  const auto temp_path = target_path.parent_path() / "recover-attachments.md.tmp";

  std::filesystem::create_directories(vault / "assets");
  std::filesystem::create_directories(vault / "docs");
  write_file_bytes(vault / "assets" / "old.png", "old-attachment-bytes");
  write_file_bytes(vault / "docs" / "new.pdf", "new-attachment-bytes");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string original =
      "# Recover Attachments\n"
      "![Old](assets/old.png)\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "recover-attachments.md",
      original.data(),
      original.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_close(handle));

  write_file_bytes(
      target_path,
      "# Recover Attachments\n"
      "[New](docs/new.pdf)\n");
  write_file_bytes(temp_path, "stale-temp");
  require_true(
      kernel::recovery::append_save_begin(
          journal_path,
          "recover-attachments-op",
          "recover-attachments.md",
          temp_path)
          .value() == 0,
      "attachment recovery SAVE_BEGIN append should succeed");

  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  sqlite3* db = open_sqlite_readonly(db_path);
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM note_attachment_refs "
          "WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='recover-attachments.md');") == 1,
      "startup recovery should clear stale attachment refs before inserting recovered refs");
  require_true(
      query_single_text(
          db,
          "SELECT attachment_rel_path FROM note_attachment_refs "
          "WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='recover-attachments.md') LIMIT 1;") ==
          "docs/new.pdf",
      "startup recovery should persist only the recovered attachment ref set");
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM attachments WHERE rel_path='docs/new.pdf' AND is_missing=0;") ==
          1,
      "startup recovery should sync recovered attachment metadata from disk truth");
  sqlite3_close(db);

  require_single_note_attachment_ref_state(
      handle,
      "recover-attachments.md",
      "docs/new.pdf",
      KERNEL_ATTACHMENT_PRESENCE_PRESENT,
      1,
      KERNEL_ATTACHMENT_KIND_PDF_LIKE,
      true,
      "startup recovery should expose the recovered attachment ref through the formal public surface");
  require_attachment_lookup_state(
      handle,
      "docs/new.pdf",
      KERNEL_ATTACHMENT_PRESENCE_PRESENT,
      1,
      KERNEL_ATTACHMENT_KIND_PDF_LIKE,
      true,
      "startup recovery should expose recovered attachment metadata through the formal public surface");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir);
}

void test_startup_recovery_plus_reopen_catch_up_removes_deleted_note_drift() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);
  const auto state_dir = state_dir_for_vault(vault);
  const auto journal_path = journal_path_for_vault(vault);
  const auto target_path = vault / "recover-delete.md";
  const auto temp_path = target_path.parent_path() / "recover-delete.md.tmp";

  std::filesystem::create_directories(vault / "assets");
  write_file_bytes(vault / "assets" / "delete.png", "delete-attachment-bytes");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string original =
      "# Recover Delete\n"
      "recover-delete-live-token\n"
      "Tags: #deletetag\n"
      "See [[DeleteLink]].\n"
      "![Delete](assets/delete.png)\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "recover-delete.md",
      original.data(),
      original.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_close(handle));

  std::filesystem::remove(target_path);
  write_file_bytes(temp_path, "stale-delete-temp");
  require_true(
      kernel::recovery::append_save_begin(
          journal_path,
          "recover-delete-op",
          "recover-delete.md",
          temp_path)
          .value() == 0,
      "deleted-note recovery SAVE_BEGIN append should succeed");

  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "deleted-note recovery test should settle to READY after reopen catch-up");

  kernel_search_results results{};
  expect_ok(kernel_search_notes(handle, "recover-delete-live-token", &results));
  require_true(
      results.count == 0,
      "reopen catch-up should remove stale search hits for a note deleted while the kernel was closed");
  kernel_free_search_results(&results);

  sqlite3* db = open_sqlite_readonly(db_path);
  require_true(
      query_single_int(db, "SELECT is_deleted FROM notes WHERE rel_path='recover-delete.md';") == 1,
      "reopen catch-up should mark the deleted note row as is_deleted=1");
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM note_tags WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='recover-delete.md');") ==
          0,
      "reopen catch-up should clear stale tags for a deleted note");
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM note_links WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='recover-delete.md');") ==
          0,
      "reopen catch-up should clear stale links for a deleted note");
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM note_attachment_refs WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='recover-delete.md');") ==
          0,
      "reopen catch-up should clear stale attachment refs for a deleted note");
  sqlite3_close(db);

  require_note_attachment_refs_not_found(
      handle,
      "recover-delete.md",
      "deleted-note recovery should remove the note from the formal attachment refs surface");
  require_attachment_lookup_not_found(
      handle,
      "assets/delete.png",
      "deleted-note recovery should remove the deleted note's attachment from the formal live catalog");
  require_attachment_referrers_not_found(
      handle,
      "assets/delete.png",
      "deleted-note recovery should remove the deleted note's attachment referrers from the formal surface");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir);
}

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

void test_startup_recovery_marks_missing_attachments_for_recovered_note_refs() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);
  const auto state_dir = state_dir_for_vault(vault);
  const auto journal_path = journal_path_for_vault(vault);
  const auto target_path = vault / "recover-missing-attachment.md";
  const auto temp_path = target_path.parent_path() / "recover-missing-attachment.md.tmp";

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string original =
      "# Recover Missing Attachment\n"
      "![Old](assets/original.png)\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "recover-missing-attachment.md",
      original.data(),
      original.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_close(handle));

  write_file_bytes(
      target_path,
      "# Recover Missing Attachment\n"
      "![Missing](assets/missing-after-recovery.png)\n");
  write_file_bytes(temp_path, "stale-temp");
  require_true(
      kernel::recovery::append_save_begin(
          journal_path,
          "recover-missing-attachment-op",
          "recover-missing-attachment.md",
          temp_path)
          .value() == 0,
      "missing-attachment recovery SAVE_BEGIN append should succeed");

  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  sqlite3* db = open_sqlite_readonly(db_path);
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM note_attachment_refs "
          "WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='recover-missing-attachment.md');") == 1,
      "startup recovery should preserve the recovered note attachment ref even when the attachment is missing");
  require_true(
      query_single_text(
          db,
          "SELECT attachment_rel_path FROM note_attachment_refs "
          "WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='recover-missing-attachment.md') LIMIT 1;") ==
          "assets/missing-after-recovery.png",
      "startup recovery should persist the recovered missing attachment ref");
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM attachments "
          "WHERE rel_path='assets/missing-after-recovery.png' AND is_missing=1;") == 1,
      "startup recovery should mark the recovered missing attachment path as missing");
  sqlite3_close(db);

  require_single_note_attachment_ref_state(
      handle,
      "recover-missing-attachment.md",
      "assets/missing-after-recovery.png",
      KERNEL_ATTACHMENT_PRESENCE_MISSING,
      1,
      KERNEL_ATTACHMENT_KIND_IMAGE_LIKE,
      false,
      "startup recovery should expose recovered missing attachment refs through the formal public surface");
  require_attachment_lookup_state(
      handle,
      "assets/missing-after-recovery.png",
      KERNEL_ATTACHMENT_PRESENCE_MISSING,
      1,
      KERNEL_ATTACHMENT_KIND_IMAGE_LIKE,
      false,
      "startup recovery should expose recovered missing attachment metadata through the formal public surface");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir);
}

void test_attachment_api_rewrite_recovery_and_rebuild_follow_live_state() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "assets");
  std::filesystem::create_directories(vault / "docs");
  write_file_bytes(vault / "assets" / "first.png", "first-bytes");
  write_file_bytes(vault / "docs" / "recovered.pdf", "recovered-bytes");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string original =
      "# Attachment Surface\n"
      "![First](assets/first.png)\n";
  expect_ok(kernel_write_note(
      handle,
      "attachment-surface.md",
      original.data(),
      original.size(),
      nullptr,
      &metadata,
      &disposition));

  const std::string rewritten =
      "# Attachment Surface\n"
      "[Recovered](docs/recovered.pdf)\n";
  expect_ok(kernel_write_note(
      handle,
      "attachment-surface.md",
      rewritten.data(),
      rewritten.size(),
      metadata.content_revision,
      &metadata,
      &disposition));

  require_single_note_attachment_ref_state(
      handle,
      "attachment-surface.md",
      "docs/recovered.pdf",
      KERNEL_ATTACHMENT_PRESENCE_PRESENT,
      1,
      KERNEL_ATTACHMENT_KIND_PDF_LIKE,
      true,
      "rewrite should replace old attachment refs in the formal public surface");

  {
    std::lock_guard lock(handle->storage_mutex);
    exec_sql(
        handle->storage.connection,
        "DELETE FROM note_attachment_refs WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='attachment-surface.md');");
    exec_sql(
        handle->storage.connection,
        "INSERT INTO note_attachment_refs(note_id, attachment_rel_path) VALUES((SELECT note_id FROM notes WHERE rel_path='attachment-surface.md'), 'assets/stale.bin');");
    exec_sql(
        handle->storage.connection,
        "UPDATE attachments SET is_missing=0 WHERE rel_path='docs/recovered.pdf';");
  }

  expect_ok(kernel_rebuild_index(handle));

  require_single_note_attachment_ref_state(
      handle,
      "attachment-surface.md",
      "docs/recovered.pdf",
      KERNEL_ATTACHMENT_PRESENCE_PRESENT,
      1,
      KERNEL_ATTACHMENT_KIND_PDF_LIKE,
      true,
      "rebuild should restore the formal live attachment ref set");
  require_attachment_lookup_state(
      handle,
      "docs/recovered.pdf",
      KERNEL_ATTACHMENT_PRESENCE_PRESENT,
      1,
      KERNEL_ATTACHMENT_KIND_PDF_LIKE,
      true,
      "rebuild should preserve formal attachment metadata");

  expect_ok(kernel_close(handle));

  prepare_state_dir_for_vault(vault);
  const auto journal_path = journal_path_for_vault(vault);
  const auto temp_path = vault / "attachment-surface.md.recovery.tmp";
  const std::string recovered =
      "# Attachment Surface\n"
      "![Recovered Missing](docs/recovered-missing.pdf)\n";
  write_file_bytes(vault / "attachment-surface.md", recovered);
  write_file_bytes(temp_path, "stale temp");
  require_true(
      !kernel::recovery::append_save_begin(
          journal_path,
          "attachment-surface-recovery-op",
          "attachment-surface.md",
          temp_path),
      "attachment surface recovery journal append should succeed");

  handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "attachment public API recovery test should settle to READY");

  require_single_note_attachment_ref_state(
      handle,
      "attachment-surface.md",
      "docs/recovered-missing.pdf",
      KERNEL_ATTACHMENT_PRESENCE_MISSING,
      1,
      KERNEL_ATTACHMENT_KIND_PDF_LIKE,
      false,
      "startup recovery should restore the recovered attachment ref in the formal public surface");
  require_attachment_lookup_state(
      handle,
      "docs/recovered-missing.pdf",
      KERNEL_ATTACHMENT_PRESENCE_MISSING,
      1,
      KERNEL_ATTACHMENT_KIND_PDF_LIKE,
      false,
      "startup recovery should expose recovered missing attachment metadata through the formal public surface");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

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

void test_rebuild_reconciles_attachment_missing_state() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "assets");
  write_file_bytes(vault / "assets" / "rebuild-attachment.png", "attachment-bytes");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "attachment rebuild test should start from a ready state");

  const std::string content =
      "# Rebuild Attachment\n"
      "![Figure](assets/rebuild-attachment.png)\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "rebuild-attachment.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));

  std::filesystem::remove(vault / "assets" / "rebuild-attachment.png");

  {
    std::lock_guard lock(handle->storage_mutex);
    exec_sql(
        handle->storage.connection,
        "UPDATE attachments SET is_missing=0 WHERE rel_path='assets/rebuild-attachment.png';");
  }

  expect_ok(kernel_rebuild_index(handle));

  sqlite3* readonly_db = open_sqlite_readonly(storage_db_for_vault(vault));
  require_true(
      query_single_int(
          readonly_db,
          "SELECT COUNT(*) FROM attachments "
          "WHERE rel_path='assets/rebuild-attachment.png' AND is_missing=1;") == 1,
      "rebuild should mark missing attachment paths from disk truth");
  require_true(
      query_single_int(
          readonly_db,
          "SELECT COUNT(*) FROM note_attachment_refs "
          "WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='rebuild-attachment.md') "
          "AND attachment_rel_path='assets/rebuild-attachment.png';") == 1,
      "rebuild should preserve note attachment refs while refreshing missing state");
  sqlite3_close(readonly_db);

  require_single_note_attachment_ref_state(
      handle,
      "rebuild-attachment.md",
      "assets/rebuild-attachment.png",
      KERNEL_ATTACHMENT_PRESENCE_MISSING,
      1,
      KERNEL_ATTACHMENT_KIND_IMAGE_LIKE,
      true,
      "rebuild should preserve the live attachment ref through the formal public surface");
  require_attachment_lookup_state(
      handle,
      "assets/rebuild-attachment.png",
      KERNEL_ATTACHMENT_PRESENCE_MISSING,
      1,
      KERNEL_ATTACHMENT_KIND_IMAGE_LIKE,
      true,
      "rebuild should refresh missing attachment metadata through the formal public surface");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_attachment_lifecycle_tests() {
  test_startup_recovery_replaces_stale_attachment_refs_and_metadata();
  test_startup_recovery_plus_reopen_catch_up_removes_deleted_note_drift();
  test_reopen_catch_up_repairs_attachment_missing_state_after_closed_window_delete();
  test_close_during_watcher_fault_backoff_leaves_attachment_create_for_reopen_catch_up();
  test_close_during_watcher_fault_backoff_leaves_attachment_delete_for_reopen_catch_up();
  test_close_during_watcher_fault_backoff_leaves_attachment_modify_for_reopen_catch_up();
  test_startup_recovery_marks_missing_attachments_for_recovered_note_refs();
  test_attachment_api_rewrite_recovery_and_rebuild_follow_live_state();
  test_attachment_api_observes_attachment_rename_reconciliation();
  test_attachment_full_rescan_reconciles_mixed_changes_through_formal_public_surface();
  test_rebuild_reconciles_attachment_missing_state();
}
