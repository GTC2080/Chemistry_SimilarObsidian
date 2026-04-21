// Reason: Keep search-facing refresh regressions together because they all verify index truth after external file changes.

#include "search/search_test_suites.h"

#include "index/refresh.h"
#include "kernel/c_api.h"
#include "search/search.h"
#include "search/search_test_support.h"
#include "support/test_support.h"
#include "third_party/sqlite/sqlite3.h"

#include <filesystem>
#include <string>
#include <vector>

namespace {

using search_tests::open_search_db;

void test_incremental_refresh_detects_external_create() {
  const auto vault = make_temp_vault();

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  expect_ok(kernel_close(handle));

  write_file_bytes(
      vault / "external-create.md",
      "# External Create\ncreated-token\n");

  auto db = open_search_db(vault);
  const std::error_code refresh_ec =
      kernel::index::refresh_markdown_path(db, vault, "external-create.md");
  require_true(!refresh_ec, "external create refresh should succeed");

  std::vector<kernel::search::SearchHit> hits;
  const std::error_code search_ec = kernel::search::search_notes(db, "created-token", hits);
  require_true(!search_ec, "external create search should succeed");
  require_true(hits.size() == 1, "external create refresh should index the new note");
  require_true(hits[0].rel_path == "external-create.md", "external create hit should preserve rel_path");
  kernel::storage::close(db);

  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_incremental_refresh_detects_external_modify() {
  const auto vault = make_temp_vault();

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string original =
      "# Before External Modify\n"
      "before-token\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "external-modify.md",
      original.data(),
      original.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_close(handle));

  write_file_bytes(
      vault / "external-modify.md",
      "# After External Modify\n"
      "after-token\n");

  auto db = open_search_db(vault);
  const std::error_code refresh_ec =
      kernel::index::refresh_markdown_path(db, vault, "external-modify.md");
  require_true(!refresh_ec, "external modify refresh should succeed");

  std::vector<kernel::search::SearchHit> hits;
  std::error_code search_ec = kernel::search::search_notes(db, "before-token", hits);
  require_true(!search_ec, "old-token search should succeed after external modify");
  require_true(hits.empty(), "external modify refresh should remove stale FTS rows");

  search_ec = kernel::search::search_notes(db, "after-token", hits);
  require_true(!search_ec, "new-token search should succeed after external modify");
  require_true(hits.size() == 1, "external modify refresh should index new file contents");

  sqlite3* readonly_db = open_sqlite_readonly(storage_db_for_vault(vault));
  require_true(
      query_single_text(readonly_db, "SELECT title FROM notes WHERE rel_path='external-modify.md';") == "After External Modify",
      "external modify refresh should replace parser-derived title");
  sqlite3_close(readonly_db);
  kernel::storage::close(db);

  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_incremental_refresh_detects_external_delete() {
  const auto vault = make_temp_vault();

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# External Delete\n"
      "delete-token\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "external-delete.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_close(handle));

  std::filesystem::remove(vault / "external-delete.md");

  auto db = open_search_db(vault);
  const std::error_code refresh_ec =
      kernel::index::refresh_markdown_path(db, vault, "external-delete.md");
  require_true(!refresh_ec, "external delete refresh should succeed");

  std::vector<kernel::search::SearchHit> hits;
  const std::error_code search_ec = kernel::search::search_notes(db, "delete-token", hits);
  require_true(!search_ec, "external delete search should succeed");
  require_true(hits.empty(), "external delete refresh should remove the note from search");

  sqlite3* readonly_db = open_sqlite_readonly(storage_db_for_vault(vault));
  require_true(
      query_single_int(readonly_db, "SELECT COUNT(*) FROM notes WHERE rel_path='external-delete.md' AND is_deleted=1;") == 1,
      "external delete refresh should mark the note deleted");
  require_true(
      query_single_int(readonly_db, "SELECT COUNT(*) FROM note_tags WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='external-delete.md');") == 0,
      "external delete refresh should clear derived tags");
  require_true(
      query_single_int(readonly_db, "SELECT COUNT(*) FROM note_links WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='external-delete.md');") == 0,
      "external delete refresh should clear derived links");
  require_true(
      query_single_int(readonly_db, "SELECT COUNT(*) FROM note_fts WHERE rowid=(SELECT note_id FROM notes WHERE rel_path='external-delete.md');") == 0,
      "external delete refresh should clear the FTS row");
  sqlite3_close(readonly_db);
  kernel::storage::close(db);

  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_incremental_refresh_marks_deleted_attachment_missing() {
  const auto vault = make_temp_vault();
  write_file_bytes(vault / "assets-diagram.png", "png-bytes");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  expect_ok(kernel_close(handle));

  auto db = open_search_db(vault);
  std::error_code refresh_ec =
      kernel::index::refresh_markdown_path(db, vault, "assets-diagram.png");
  require_true(!refresh_ec, "attachment create refresh should succeed");

  sqlite3* readonly_db = open_sqlite_readonly(storage_db_for_vault(vault));
  require_true(
      query_single_int(
          readonly_db,
          "SELECT COUNT(*) FROM attachments WHERE rel_path='assets-diagram.png' AND is_missing=0;") == 1,
      "attachment refresh should register a present attachment");
  sqlite3_close(readonly_db);

  std::filesystem::remove(vault / "assets-diagram.png");

  refresh_ec = kernel::index::refresh_markdown_path(db, vault, "assets-diagram.png");
  require_true(!refresh_ec, "attachment delete refresh should succeed");

  readonly_db = open_sqlite_readonly(storage_db_for_vault(vault));
  require_true(
      query_single_int(
          readonly_db,
          "SELECT COUNT(*) FROM attachments WHERE rel_path='assets-diagram.png' AND is_missing=1;") == 1,
      "attachment delete refresh should mark attachment missing");
  sqlite3_close(readonly_db);

  kernel::storage::close(db);
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_search_incremental_refresh_tests() {
  test_incremental_refresh_detects_external_create();
  test_incremental_refresh_detects_external_modify();
  test_incremental_refresh_detects_external_delete();
  test_incremental_refresh_marks_deleted_attachment_missing();
}
