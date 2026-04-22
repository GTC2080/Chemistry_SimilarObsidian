// Reason: Keep search note persistence and recovery regressions together because they all verify index truth after writes or startup recovery.

#include "search/search_note_core_suites.h"

#include "kernel/c_api.h"
#include "recovery/journal.h"
#include "search/search.h"
#include "search/search_test_support.h"
#include "support/test_support.h"
#include "third_party/sqlite/sqlite3.h"

#include <filesystem>
#include <string>
#include <vector>

namespace {

using search_tests::open_search_db;

void test_search_finds_written_note_by_body_term() {
  const auto vault = make_temp_vault();

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Search Title\n"
      "Unique body token: benzaldehyde.\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "search.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_close(handle));

  auto db = open_search_db(vault);
  std::vector<kernel::search::SearchHit> hits;
  const std::error_code ec = kernel::search::search_notes(db, "benzaldehyde", hits);
  require_true(!ec, "search should succeed");
  require_true(hits.size() == 1, "search should return one matching note");
  require_true(hits[0].rel_path == "search.md", "search hit should preserve rel_path");
  require_true(hits[0].title == "Search Title", "search hit should preserve parser title");
  kernel::storage::close(db);

  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_search_rewrite_replaces_old_fts_rows() {
  const auto vault = make_temp_vault();

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata first{};
  kernel_write_disposition disposition{};
  const std::string original = "# Title\nalpha token\n";
  expect_ok(kernel_write_note(
      handle,
      "rewrite-search.md",
      original.data(),
      original.size(),
      nullptr,
      &first,
      &disposition));

  kernel_note_metadata second{};
  const std::string rewritten = "# Title\nbeta token\n";
  expect_ok(kernel_write_note(
      handle,
      "rewrite-search.md",
      rewritten.data(),
      rewritten.size(),
      first.content_revision,
      &second,
      &disposition));
  expect_ok(kernel_close(handle));

  auto db = open_search_db(vault);
  std::vector<kernel::search::SearchHit> hits;
  std::error_code ec = kernel::search::search_notes(db, "alpha", hits);
  require_true(!ec, "search should succeed for old token query");
  require_true(hits.empty(), "rewrite should remove old FTS rows");

  ec = kernel::search::search_notes(db, "beta", hits);
  require_true(!ec, "search should succeed for new token query");
  require_true(hits.size() == 1, "rewrite should preserve only the new FTS row");
  require_true(hits[0].rel_path == "rewrite-search.md", "rewritten search hit should preserve rel_path");
  kernel::storage::close(db);

  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_search_no_op_preserves_single_fts_row() {
  const auto vault = make_temp_vault();

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content = "# Stable Title\nstable token\n";
  kernel_note_metadata first{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "stable.md",
      content.data(),
      content.size(),
      nullptr,
      &first,
      &disposition));
  require_true(disposition == KERNEL_WRITE_WRITTEN, "first write should persist the note");

  kernel_note_metadata second{};
  expect_ok(kernel_write_note(
      handle,
      "stable.md",
      content.data(),
      content.size(),
      first.content_revision,
      &second,
      &disposition));
  require_true(disposition == KERNEL_WRITE_NO_OP, "same-content rewrite should be NO_OP");
  expect_ok(kernel_close(handle));

  auto db = open_search_db(vault);
  std::vector<kernel::search::SearchHit> hits;
  const std::error_code ec = kernel::search::search_notes(db, "stable", hits);
  require_true(!ec, "search should succeed for stable token");
  require_true(hits.size() == 1, "no-op rewrite should not duplicate FTS hits");
  kernel::storage::close(db);

  sqlite3* readonly_db = open_sqlite_readonly(storage_db_for_vault(vault));
  require_true(
      query_single_int(readonly_db, "SELECT COUNT(*) FROM note_fts;") == 1,
      "no-op rewrite should keep exactly one note_fts row");
  sqlite3_close(readonly_db);

  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_search_startup_recovery_replaces_stale_fts_rows() {
  const auto vault = make_temp_vault();
  const auto state_dir = state_dir_for_vault(vault);
  const auto journal_path = journal_path_for_vault(vault);
  const auto target_path = vault / "recover-search.md";
  const auto temp_path = target_path.parent_path() / "recover-search.md.codex-recovery.tmp";

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata original_metadata{};
  kernel_write_disposition disposition{};
  const std::string original = "# Search Recovery\nalpha token\n";
  expect_ok(kernel_write_note(
      handle,
      "recover-search.md",
      original.data(),
      original.size(),
      nullptr,
      &original_metadata,
      &disposition));
  expect_ok(kernel_close(handle));

  const std::string recovered = "# Search Recovery\nbeta token\n";
  write_file_bytes(target_path, recovered);
  write_file_bytes(temp_path, "stale-temp");
  const std::error_code append_ec = kernel::recovery::append_save_begin(
      journal_path,
      "manual-search-recovery",
      "recover-search.md",
      temp_path);
  require_true(!append_ec, "manual SAVE_BEGIN append should succeed");

  handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  expect_ok(kernel_close(handle));

  auto db = open_search_db(vault);
  std::vector<kernel::search::SearchHit> hits;
  std::error_code ec = kernel::search::search_notes(db, "alpha", hits);
  require_true(!ec, "old-token search should succeed after recovery");
  require_true(hits.empty(), "startup recovery should replace stale FTS rows");

  ec = kernel::search::search_notes(db, "beta", hits);
  require_true(!ec, "new-token search should succeed after recovery");
  require_true(hits.size() == 1, "startup recovery should index recovered body text");
  require_true(hits[0].rel_path == "recover-search.md", "recovered hit should preserve rel_path");
  kernel::storage::close(db);

  sqlite3* readonly_db = open_sqlite_readonly(storage_db_for_vault(vault));
  require_true(
      query_single_int(readonly_db, "SELECT COUNT(*) FROM note_fts;") == 1,
      "startup recovery should keep a single note_fts row for the note");
  sqlite3_close(readonly_db);
  require_true(!std::filesystem::exists(temp_path), "startup recovery should clean stale temp file");

  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir);
}

}  // namespace

void run_search_note_persistence_tests() {
  test_search_finds_written_note_by_body_term();
  test_search_rewrite_replaces_old_fts_rows();
  test_search_no_op_preserves_single_fts_row();
  test_search_startup_recovery_replaces_stale_fts_rows();
}
