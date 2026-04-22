// Reason: This file isolates attachment startup recovery coverage so reopen catch-up repair tests can stay separate.

#include "kernel/c_api.h"

#include "api/kernel_api_attachment_lifecycle_recovery_suites.h"
#include "api/kernel_api_test_support.h"
#include "recovery/journal.h"
#include "support/test_support.h"
#include "third_party/sqlite/sqlite3.h"

#include <filesystem>
#include <string>

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

}  // namespace

void run_attachment_lifecycle_recovery_startup_tests() {
  test_startup_recovery_replaces_stale_attachment_refs_and_metadata();
  test_startup_recovery_plus_reopen_catch_up_removes_deleted_note_drift();
  test_startup_recovery_marks_missing_attachments_for_recovered_note_refs();
}
