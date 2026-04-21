// Reason: This file isolates formal attachment public-surface regressions so the main API suite can separate legacy and formal attachment coverage.

#include "kernel/c_api.h"

#include "api/kernel_api_test_support.h"
#include "core/kernel_internal.h"
#include "index/refresh.h"
#include "recovery/journal.h"
#include "storage/storage.h"
#include "support/test_support.h"
#include "watcher/integration.h"

#include <filesystem>
#include <string>
#include <vector>

namespace {

void test_attachment_public_surface_lists_live_catalog_and_single_attachment() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "assets");
  std::filesystem::create_directories(vault / "docs");
  std::filesystem::create_directories(vault / "misc");
  write_file_bytes(vault / "assets" / "present.png", "present-bytes");
  write_file_bytes(vault / "docs" / "shared.pdf", "shared-bytes");
  write_file_bytes(vault / "misc" / "unreferenced.bin", "unreferenced-bytes");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string first =
      "# First Attachment Surface\n"
      "![Present](assets/present.png)\n"
      "[Missing](docs/missing.pdf)\n";
  expect_ok(kernel_write_note(
      handle,
      "first-surface.md",
      first.data(),
      first.size(),
      nullptr,
      &metadata,
      &disposition));

  const std::string second =
      "# Second Attachment Surface\n"
      "![Present](assets/present.png)\n"
      "[Shared](docs/shared.pdf)\n";
  expect_ok(kernel_write_note(
      handle,
      "second-surface.md",
      second.data(),
      second.size(),
      nullptr,
      &metadata,
      &disposition));

  kernel_attachment_list attachments{};
  expect_ok(kernel_query_attachments(handle, static_cast<size_t>(-1), &attachments));
  require_true(attachments.count == 3, "attachment public surface should list exactly the live catalog");

  require_true(
      std::string(attachments.attachments[0].rel_path) == "assets/present.png",
      "attachment public surface should sort the live catalog by rel_path");
  require_true(
      std::string(attachments.attachments[1].rel_path) == "docs/missing.pdf",
      "attachment public surface should keep missing live attachments in rel_path order");
  require_true(
      std::string(attachments.attachments[2].rel_path) == "docs/shared.pdf",
      "attachment public surface should include every live attachment exactly once");

  require_true(
      std::string(attachments.attachments[0].basename) == "present.png",
      "attachment public surface should expose basename for list entries");
  require_true(
      std::string(attachments.attachments[0].extension) == ".png",
      "attachment public surface should expose lowercase extensions for list entries");
  require_true(
      attachments.attachments[0].kind == KERNEL_ATTACHMENT_KIND_IMAGE_LIKE,
      "attachment public surface should classify common image attachments");
  require_true(
      attachments.attachments[0].presence == KERNEL_ATTACHMENT_PRESENCE_PRESENT,
      "attachment public surface should report present attachments");
  require_true(
      attachments.attachments[0].file_size > 0,
      "attachment public surface should expose reconciled file size for present attachments");
  require_true(
      attachments.attachments[0].mtime_ns > 0,
      "attachment public surface should expose reconciled mtime for present attachments");
  require_true(
      attachments.attachments[0].ref_count == 2,
      "attachment public surface should report the live note ref count");
  require_true(
      attachments.attachments[0].flags == KERNEL_ATTACHMENT_FLAG_NONE,
      "attachment public surface should keep undefined flags cleared");

  require_true(
      attachments.attachments[1].kind == KERNEL_ATTACHMENT_KIND_PDF_LIKE,
      "attachment public surface should classify common PDF attachments");
  require_true(
      attachments.attachments[1].presence == KERNEL_ATTACHMENT_PRESENCE_MISSING,
      "attachment public surface should preserve live missing attachments");
  require_true(
      attachments.attachments[1].file_size == 0,
      "attachment public surface should allow zero file size for missing attachments never seen on disk");
  require_true(
      attachments.attachments[1].mtime_ns == 0,
      "attachment public surface should allow zero mtime for missing attachments never seen on disk");
  require_true(
      attachments.attachments[1].ref_count == 1,
      "attachment public surface should track missing attachment ref counts");

  kernel_attachment_record attachment{};
  expect_ok(kernel_get_attachment(handle, "assets/present.png", &attachment));
  require_true(
      std::string(attachment.rel_path) == "assets/present.png",
      "single attachment lookup should return the normalized rel_path");
  require_true(
      std::string(attachment.basename) == "present.png",
      "single attachment lookup should expose basename");
  require_true(
      std::string(attachment.extension) == ".png",
      "single attachment lookup should expose lowercase extension");
  require_true(
      attachment.kind == KERNEL_ATTACHMENT_KIND_IMAGE_LIKE,
      "single attachment lookup should expose attachment kind");
  require_true(
      attachment.presence == KERNEL_ATTACHMENT_PRESENCE_PRESENT,
      "single attachment lookup should preserve present state");
  require_true(
      attachment.file_size == attachments.attachments[0].file_size,
      "single attachment lookup should preserve present file size");
  require_true(
      attachment.mtime_ns == attachments.attachments[0].mtime_ns,
      "single attachment lookup should preserve present mtime");
  require_true(
      attachment.ref_count == 2,
      "single attachment lookup should preserve the live ref count");
  kernel_free_attachment_record(&attachment);

  expect_ok(kernel_get_attachment(handle, "docs/missing.pdf", &attachment));
  require_true(
      attachment.presence == KERNEL_ATTACHMENT_PRESENCE_MISSING,
      "single attachment lookup should preserve live missing state");
  require_true(
      attachment.file_size == 0,
      "single attachment lookup should preserve zero file size for never-observed missing rows");
  require_true(
      attachment.mtime_ns == 0,
      "single attachment lookup should preserve zero mtime for never-observed missing rows");
  require_true(
      attachment.kind == KERNEL_ATTACHMENT_KIND_PDF_LIKE,
      "single attachment lookup should preserve attachment kind for missing rows");
  kernel_free_attachment_record(&attachment);

  require_true(
      kernel_get_attachment(handle, "misc/unreferenced.bin", &attachment).code ==
          KERNEL_ERROR_NOT_FOUND,
      "single attachment lookup should reject disk files that are not in the live catalog");

  kernel_free_attachment_list(&attachments);
  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_attachment_public_surface_metadata_contract_covers_kind_mapping_and_missing_carry_forward() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "assets");
  std::filesystem::create_directories(vault / "chem");
  std::filesystem::create_directories(vault / "misc");
  write_file_bytes(vault / "assets" / "data.bin", "12345");
  write_file_bytes(vault / "chem" / "ligand.CDX", "chem");
  write_file_bytes(vault / "misc" / "noext", "noext");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string content =
      "# Metadata Attachment Surface\n"
      "[Generic](assets/data.bin)\n"
      "[Chem](chem/ligand.CDX)\n";
  expect_ok(kernel_write_note(
      handle,
      "metadata-attachments.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));
  {
    std::lock_guard lock(handle->storage_mutex);
    require_true(
        !kernel::index::refresh_markdown_path(handle->storage, vault, "misc/noext"),
        "metadata contract test should refresh extensionless attachment metadata");
    exec_sql(
        handle->storage.connection,
        "INSERT INTO note_attachment_refs(note_id, attachment_rel_path) "
        "VALUES((SELECT note_id FROM notes WHERE rel_path='metadata-attachments.md'), 'misc/noext');");
  }

  kernel_attachment_list attachments{};
  expect_ok(kernel_query_attachments(handle, static_cast<size_t>(-1), &attachments));
  require_true(attachments.count == 3, "metadata contract test should see all three live attachments");

  require_true(
      std::string(attachments.attachments[0].rel_path) == "assets/data.bin" &&
          attachments.attachments[0].kind == KERNEL_ATTACHMENT_KIND_GENERIC_FILE &&
          std::string(attachments.attachments[0].extension) == ".bin" &&
          attachments.attachments[0].file_size == 5 &&
          attachments.attachments[0].mtime_ns > 0,
      "metadata contract should freeze generic-file kind and stable present metadata");
  require_true(
      std::string(attachments.attachments[1].rel_path) == "chem/ligand.CDX" &&
          attachments.attachments[1].kind == KERNEL_ATTACHMENT_KIND_CHEM_LIKE &&
          std::string(attachments.attachments[1].extension) == ".cdx" &&
          attachments.attachments[1].file_size == 4 &&
          attachments.attachments[1].mtime_ns > 0,
      "metadata contract should normalize extensions and classify chem-like attachments");
  require_true(
      std::string(attachments.attachments[2].rel_path) == "misc/noext" &&
          attachments.attachments[2].kind == KERNEL_ATTACHMENT_KIND_UNKNOWN &&
          std::string(attachments.attachments[2].extension).empty() &&
          attachments.attachments[2].file_size == 5 &&
          attachments.attachments[2].mtime_ns > 0,
      "metadata contract should classify extensionless attachments as unknown");

  const auto preserved_size = attachments.attachments[0].file_size;
  const auto preserved_mtime = attachments.attachments[0].mtime_ns;
  kernel_free_attachment_list(&attachments);

  std::filesystem::remove(vault / "assets" / "data.bin");
  require_eventually(
      [&]() {
        kernel_attachment_record attachment{};
        const kernel_status status = kernel_get_attachment(handle, "assets/data.bin", &attachment);
        if (status.code != KERNEL_OK) {
          return false;
        }

        const bool matches =
            attachment.presence == KERNEL_ATTACHMENT_PRESENCE_MISSING &&
            attachment.kind == KERNEL_ATTACHMENT_KIND_GENERIC_FILE &&
            std::string(attachment.extension) == ".bin" &&
            attachment.file_size == preserved_size &&
            attachment.mtime_ns == preserved_mtime;
        kernel_free_attachment_record(&attachment);
        return matches;
      },
      "metadata contract should preserve last present size and mtime after watcher delete");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

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
    exec_sql(handle->storage.connection, "DELETE FROM note_attachment_refs WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='attachment-surface.md');");
    exec_sql(handle->storage.connection, "INSERT INTO note_attachment_refs(note_id, attachment_rel_path) VALUES((SELECT note_id FROM notes WHERE rel_path='attachment-surface.md'), 'assets/stale.bin');");
    exec_sql(handle->storage.connection, "UPDATE attachments SET is_missing=0 WHERE rel_path='docs/recovered.pdf';");
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
  require_true(!kernel::storage::open_or_create(storage_db_for_vault(vault), db), "attachment rename API test should open storage db");
  require_true(!kernel::storage::ensure_schema_v1(db), "attachment rename API test should ensure schema");

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

void run_attachment_public_surface_tests() {
  test_attachment_public_surface_lists_live_catalog_and_single_attachment();
  test_attachment_public_surface_metadata_contract_covers_kind_mapping_and_missing_carry_forward();
  test_attachment_public_surface_note_refs_and_referrers_are_stable();
  test_attachment_public_surface_excludes_orphaned_paths_and_matches_search();
  test_attachment_api_rewrite_recovery_and_rebuild_follow_live_state();
  test_attachment_api_observes_attachment_rename_reconciliation();
  test_attachment_full_rescan_reconciles_mixed_changes_through_formal_public_surface();
}
