// Reason: This file isolates parser-derived storage contract tests so the broader core contract suite stays easier to scan.

#include "kernel/c_api.h"

#include "api/kernel_api_core_base_suites.h"
#include "api/kernel_api_test_support.h"
#include "support/test_support.h"
#include "third_party/sqlite/sqlite3.h"

#include <filesystem>
#include <string>

namespace {

void test_write_persists_parser_derived_tags_and_wikilinks() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Parser Title\n"
      "See [[Alpha]] and [[Beta|Shown]].\n"
      "Tags: #chem #org\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "derived.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));

  sqlite3* db = open_sqlite_readonly(db_path);
  require_true(
      query_single_text(db, "SELECT title FROM notes WHERE rel_path='derived.md';") == "Parser Title",
      "write should persist parser-derived title");
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM note_tags WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='derived.md');") == 2,
      "write should persist two parser-derived tags");
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM note_links WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='derived.md');") == 2,
      "write should persist two parser-derived wikilinks");
  require_true(
      query_single_text(
          db,
          "SELECT tag FROM note_tags WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='derived.md') ORDER BY rowid LIMIT 1;") == "chem",
      "tags should preserve parser order in storage");
  require_true(
      query_single_text(
          db,
          "SELECT target FROM note_links WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='derived.md') ORDER BY rowid LIMIT 1;") == "Alpha",
      "wikilinks should preserve parser order in storage");
  sqlite3_close(db);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_rewrite_replaces_old_parser_derived_rows() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string original =
      "# Original Title\n"
      "See [[Alpha]] and [[Beta]].\n"
      "Tags: #chem #org\n";
  kernel_note_metadata first{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "rewrite.md",
      original.data(),
      original.size(),
      nullptr,
      &first,
      &disposition));

  const std::string rewritten =
      "No heading now.\n"
      "See [[Gamma]].\n"
      "Tags: #newtag\n";
  kernel_note_metadata second{};
  expect_ok(kernel_write_note(
      handle,
      "rewrite.md",
      rewritten.data(),
      rewritten.size(),
      first.content_revision,
      &second,
      &disposition));

  sqlite3* db = open_sqlite_readonly(db_path);
  require_true(
      query_single_text(db, "SELECT title FROM notes WHERE rel_path='rewrite.md';") == "rewrite",
      "rewrite should replace parser title with filename fallback when heading disappears");
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM note_tags WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='rewrite.md');") == 1,
      "rewrite should clear old tags before inserting new ones");
  require_true(
      query_single_text(
          db,
          "SELECT tag FROM note_tags WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='rewrite.md') LIMIT 1;") == "newtag",
      "rewrite should persist only the new tag set");
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM note_links WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='rewrite.md');") == 1,
      "rewrite should clear old wikilinks before inserting new ones");
  require_true(
      query_single_text(
          db,
          "SELECT target FROM note_links WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='rewrite.md') LIMIT 1;") == "Gamma",
      "rewrite should persist only the new wikilink set");
  sqlite3_close(db);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_kernel_api_core_parser_contract_tests() {
  test_write_persists_parser_derived_tags_and_wikilinks();
  test_rewrite_replaces_old_parser_derived_rows();
}
