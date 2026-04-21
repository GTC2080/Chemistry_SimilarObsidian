// Reason: Keep watcher rename/move continuity regressions together because they verify identity-preserving reconciliation paths.

#include "watcher/watcher_test_suites.h"

#include "watcher/integration.h"
#include "watcher/session.h"
#include "watcher/watcher.h"
#include "watcher/watcher_test_support.h"
#include "index/refresh.h"
#include "kernel/c_api.h"
#include "search/search.h"
#include "support/test_support.h"

#include <filesystem>
#include <string>
#include <vector>

namespace {

using kernel::watcher::CoalescedAction;
using kernel::watcher::CoalescedActionKind;
using watcher_tests::make_temp_watch_root;
using watcher_tests::open_search_db;

void test_poll_and_refresh_once_preserves_note_identity_across_external_rename() {
  const auto root = make_temp_watch_root();
  kernel_handle* handle = nullptr;
  require_true(kernel_open_vault(root.string().c_str(), &handle).code == KERNEL_OK, "vault should open");

  const std::string content =
      "body carries rename-watch-token\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  require_true(
      kernel_write_note(handle, "old-name.md", content.data(), content.size(), nullptr, &metadata, &disposition).code == KERNEL_OK,
      "seed note write should succeed");
  require_true(kernel_close(handle).code == KERNEL_OK, "vault should close");

  auto db = open_search_db(root);
  const int old_note_id =
      query_single_int(db.connection, "SELECT note_id FROM notes WHERE rel_path='old-name.md' AND is_deleted=0;");
  kernel::watcher::WatchSession session;
  require_true(!kernel::watcher::open_watch_session(root, session), "watch session should open");

  std::filesystem::rename(root / "old-name.md", root / "new-name.md");

  std::vector<CoalescedAction> actions;
  const std::error_code ec = kernel::watcher::poll_and_refresh_once(session, db, 1000, actions);
  require_true(!ec, "watch rename integration should succeed");
  require_true(actions.size() == 1, "watch rename integration should yield one rename action");
  require_true(actions[0].kind == CoalescedActionKind::RenamePath, "rename should coalesce to rename continuity action");
  require_true(actions[0].rel_path == "old-name.md", "rename action should preserve old path");
  require_true(actions[0].secondary_rel_path == "new-name.md", "rename action should preserve new path");

  std::vector<kernel::search::SearchHit> hits;
  require_true(!kernel::search::search_notes(db, "rename-watch-token", hits), "rename token search should succeed");
  require_true(hits.size() == 1, "rename integration should keep one search hit");
  require_true(hits[0].rel_path == "new-name.md", "rename integration should move the hit to the new rel_path");

  require_true(!kernel::search::search_notes(db, "old-name", hits), "old fallback title query should succeed");
  require_true(hits.empty(), "old fallback title should disappear after rename");
  require_true(!kernel::search::search_notes(db, "new-name", hits), "new fallback title query should succeed");
  require_true(hits.size() == 1, "new fallback title should be searchable after rename");
  require_true(hits[0].rel_path == "new-name.md", "new fallback title hit should point to new rel_path");

  sqlite3* readonly_db = open_sqlite_readonly(storage_db_for_vault(root));
  require_true(
      query_single_int(readonly_db, "SELECT COUNT(*) FROM notes WHERE rel_path='old-name.md';") == 0,
      "rename continuity should remove the old rel_path row");
  require_true(
      query_single_int(readonly_db, "SELECT COUNT(*) FROM notes WHERE rel_path='new-name.md' AND is_deleted=0;") == 1,
      "rename integration should add the new path as active");
  require_true(
      query_single_int(readonly_db, "SELECT note_id FROM notes WHERE rel_path='new-name.md' AND is_deleted=0;") == old_note_id,
      "rename continuity should preserve note identity");
  sqlite3_close(readonly_db);

  kernel::watcher::close_watch_session(session);
  kernel::storage::close(db);
  std::filesystem::remove_all(root);
  std::filesystem::remove_all(state_dir_for_vault(root));
}

void test_apply_actions_preserves_note_identity_across_move() {
  const auto root = make_temp_watch_root();
  std::filesystem::create_directories(root / "from");
  std::filesystem::create_directories(root / "to");
  kernel_handle* handle = nullptr;
  require_true(kernel_open_vault(root.string().c_str(), &handle).code == KERNEL_OK, "vault should open");

  const std::string content =
      "body carries move-watch-token\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  require_true(
      kernel_write_note(handle, "from/move-me.md", content.data(), content.size(), nullptr, &metadata, &disposition).code == KERNEL_OK,
      "seed move note write should succeed");
  require_true(kernel_close(handle).code == KERNEL_OK, "vault should close");

  auto db = open_search_db(root);
  const int old_note_id =
      query_single_int(db.connection, "SELECT note_id FROM notes WHERE rel_path='from/move-me.md' AND is_deleted=0;");
  std::filesystem::rename(root / "from" / "move-me.md", root / "to" / "move-me.md");

  const std::vector<CoalescedAction> actions = {
      {CoalescedActionKind::RenamePath, "from/move-me.md", "to/move-me.md"}};
  const std::error_code ec = kernel::watcher::apply_actions(db, root, actions);
  require_true(!ec, "move continuity apply should succeed");
  require_true(actions[0].rel_path == "from/move-me.md", "move action should preserve old path");
  require_true(actions[0].secondary_rel_path == "to/move-me.md", "move action should preserve new path");

  sqlite3* readonly_db = open_sqlite_readonly(storage_db_for_vault(root));
  require_true(
      query_single_int(readonly_db, "SELECT COUNT(*) FROM notes WHERE rel_path='from/move-me.md';") == 0,
      "move continuity should remove old rel_path");
  require_true(
      query_single_int(readonly_db, "SELECT note_id FROM notes WHERE rel_path='to/move-me.md' AND is_deleted=0;") == old_note_id,
      "move continuity should preserve note identity");
  sqlite3_close(readonly_db);

  kernel::storage::close(db);
  std::filesystem::remove_all(root);
  std::filesystem::remove_all(state_dir_for_vault(root));
}

void test_apply_actions_reconciles_attachment_rename() {
  const auto root = make_temp_watch_root();
  std::filesystem::create_directories(root / "assets");
  write_file_bytes(root / "assets" / "old.bin", "old-attachment");

  kernel_handle* handle = nullptr;
  require_true(kernel_open_vault(root.string().c_str(), &handle).code == KERNEL_OK, "vault should open");
  require_true(kernel_close(handle).code == KERNEL_OK, "vault should close");

  auto db = open_search_db(root);
  require_true(!kernel::index::refresh_markdown_path(db, root, "assets/old.bin"), "seed attachment refresh should succeed");

  std::filesystem::rename(root / "assets" / "old.bin", root / "assets" / "renamed.bin");

  const std::vector<CoalescedAction> actions = {
      {CoalescedActionKind::RenamePath, "assets/old.bin", "assets/renamed.bin"}};
  const std::error_code ec = kernel::watcher::apply_actions(db, root, actions);
  require_true(!ec, "attachment rename apply should succeed");

  sqlite3* readonly_db = open_sqlite_readonly(storage_db_for_vault(root));
  require_true(
      query_single_int(readonly_db, "SELECT COUNT(*) FROM attachments WHERE rel_path='assets/old.bin' AND is_missing=1;") == 1,
      "attachment rename should mark old attachment path missing");
  require_true(
      query_single_int(readonly_db, "SELECT COUNT(*) FROM attachments WHERE rel_path='assets/renamed.bin' AND is_missing=0;") == 1,
      "attachment rename should register new attachment path present");
  sqlite3_close(readonly_db);

  kernel::storage::close(db);
  std::filesystem::remove_all(root);
  std::filesystem::remove_all(state_dir_for_vault(root));
}

}  // namespace

void run_watcher_continuity_integration_tests() {
  test_poll_and_refresh_once_preserves_note_identity_across_external_rename();
  test_apply_actions_preserves_note_identity_across_move();
  test_apply_actions_reconciles_attachment_rename();
}
