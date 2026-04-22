// Reason: This file isolates formal attachment public-surface lifecycle checks so rewrite, recovery, and rebuild expectations stay easy to scan.

#include "kernel/c_api.h"

#include "api/kernel_api_attachment_lifecycle_suites.h"
#include "api/kernel_api_test_support.h"
#include "core/kernel_internal.h"
#include "recovery/journal.h"
#include "support/test_support.h"
#include "third_party/sqlite/sqlite3.h"

#include <filesystem>
#include <mutex>
#include <string>

namespace {

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

void run_attachment_lifecycle_surface_tests() {
  test_attachment_api_rewrite_recovery_and_rebuild_follow_live_state();
  test_rebuild_reconciles_attachment_missing_state();
}
