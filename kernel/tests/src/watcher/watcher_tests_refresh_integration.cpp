// Reason: Keep watcher refresh/full-rescan integration regressions together because they verify disk-to-index reconciliation after external changes.

#include "watcher/watcher_test_suites.h"

#include "watcher/integration.h"
#include "watcher/session.h"
#include "watcher/watcher.h"
#include "watcher/watcher_test_support.h"
#include "kernel/c_api.h"
#include "search/search.h"
#include "support/test_support.h"
#include "third_party/sqlite/sqlite3.h"

#include <filesystem>
#include <string>
#include <vector>

namespace {

using kernel::watcher::CoalescedAction;
using kernel::watcher::CoalescedActionKind;
using watcher_tests::make_temp_watch_root;
using watcher_tests::open_search_db;

void test_poll_and_refresh_once_indexes_external_create() {
  const auto root = make_temp_watch_root();
  kernel_handle* handle = nullptr;
  require_true(kernel_open_vault(root.string().c_str(), &handle).code == KERNEL_OK, "vault should open");
  require_true(kernel_close(handle).code == KERNEL_OK, "vault should close");

  auto db = open_search_db(root);
  kernel::watcher::WatchSession session;
  require_true(!kernel::watcher::open_watch_session(root, session), "watch session should open");

  write_file_bytes(
      root / "watch-created.md",
      "# Watch Created\nwatch-created-token\n");

  std::vector<CoalescedAction> actions;
  const std::error_code ec = kernel::watcher::poll_and_refresh_once(session, db, 1000, actions);
  require_true(!ec, "watch create integration should succeed");
  require_true(actions.size() == 1, "watch create integration should yield one action");
  require_true(actions[0].kind == CoalescedActionKind::RefreshPath, "watch create integration should refresh");

  std::vector<kernel::search::SearchHit> hits;
  require_true(!kernel::search::search_notes(db, "watch-created-token", hits), "watch create search should succeed");
  require_true(hits.size() == 1, "watch create integration should index the note");
  require_true(hits[0].rel_path == "watch-created.md", "watch create hit should preserve rel_path");

  kernel::watcher::close_watch_session(session);
  kernel::storage::close(db);
  std::filesystem::remove_all(root);
  std::filesystem::remove_all(state_dir_for_vault(root));
}

void test_poll_and_refresh_once_updates_external_modify() {
  const auto root = make_temp_watch_root();
  kernel_handle* handle = nullptr;
  require_true(kernel_open_vault(root.string().c_str(), &handle).code == KERNEL_OK, "vault should open");

  const std::string original =
      "# Before Watch Modify\n"
      "before-watch-token\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  require_true(
      kernel_write_note(handle, "watch-modify.md", original.data(), original.size(), nullptr, &metadata, &disposition).code == KERNEL_OK,
      "seed note write should succeed");
  require_true(kernel_close(handle).code == KERNEL_OK, "vault should close");

  auto db = open_search_db(root);
  kernel::watcher::WatchSession session;
  require_true(!kernel::watcher::open_watch_session(root, session), "watch session should open");

  write_file_bytes(
      root / "watch-modify.md",
      "# After Watch Modify\n"
      "after-watch-token\n");

  std::vector<CoalescedAction> actions;
  const std::error_code ec = kernel::watcher::poll_and_refresh_once(session, db, 1000, actions);
  require_true(!ec, "watch modify integration should succeed");
  require_true(actions.size() == 1, "watch modify integration should yield one action");
  require_true(actions[0].kind == CoalescedActionKind::RefreshPath, "watch modify integration should refresh");

  std::vector<kernel::search::SearchHit> hits;
  require_true(!kernel::search::search_notes(db, "before-watch-token", hits), "old watch token search should succeed");
  require_true(hits.empty(), "watch modify integration should remove stale FTS rows");
  require_true(!kernel::search::search_notes(db, "after-watch-token", hits), "new watch token search should succeed");
  require_true(hits.size() == 1, "watch modify integration should index new content");

  kernel::watcher::close_watch_session(session);
  kernel::storage::close(db);
  std::filesystem::remove_all(root);
  std::filesystem::remove_all(state_dir_for_vault(root));
}

void test_poll_and_refresh_once_applies_external_delete() {
  const auto root = make_temp_watch_root();
  kernel_handle* handle = nullptr;
  require_true(kernel_open_vault(root.string().c_str(), &handle).code == KERNEL_OK, "vault should open");

  const std::string content =
      "# Before Watch Delete\n"
      "watch-delete-token\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  require_true(
      kernel_write_note(handle, "watch-delete.md", content.data(), content.size(), nullptr, &metadata, &disposition).code == KERNEL_OK,
      "seed note write should succeed");
  require_true(kernel_close(handle).code == KERNEL_OK, "vault should close");

  auto db = open_search_db(root);
  kernel::watcher::WatchSession session;
  require_true(!kernel::watcher::open_watch_session(root, session), "watch session should open");

  std::filesystem::remove(root / "watch-delete.md");

  std::vector<CoalescedAction> actions;
  const std::error_code ec = kernel::watcher::poll_and_refresh_once(session, db, 1000, actions);
  require_true(!ec, "watch delete integration should succeed");
  require_true(actions.size() == 1, "watch delete integration should yield one action");
  require_true(actions[0].kind == CoalescedActionKind::DeletePath, "watch delete integration should delete");

  std::vector<kernel::search::SearchHit> hits;
  require_true(!kernel::search::search_notes(db, "watch-delete-token", hits), "watch delete search should succeed");
  require_true(hits.empty(), "watch delete integration should remove the note from search");

  sqlite3* readonly_db = nullptr;
  require_true(
      sqlite3_open_v2(storage_db_for_vault(root).string().c_str(), &readonly_db, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK,
      "readonly sqlite db should open");
  require_true(
      query_single_int(readonly_db, "SELECT COUNT(*) FROM notes WHERE rel_path='watch-delete.md' AND is_deleted=1;") == 1,
      "watch delete integration should mark note deleted");
  sqlite3_close(readonly_db);

  kernel::watcher::close_watch_session(session);
  kernel::storage::close(db);
  std::filesystem::remove_all(root);
  std::filesystem::remove_all(state_dir_for_vault(root));
}

void test_apply_actions_full_rescan_reconciles_mixed_changes() {
  const auto root = make_temp_watch_root();
  kernel_handle* handle = nullptr;
  require_true(kernel_open_vault(root.string().c_str(), &handle).code == KERNEL_OK, "vault should open");

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string keep_original =
      "# Keep Before\n"
      "keep-before-token\n";
  const std::string delete_original =
      "# Delete Before\n"
      "delete-before-token\n";
  require_true(
      kernel_write_note(handle, "keep.md", keep_original.data(), keep_original.size(), nullptr, &metadata, &disposition).code == KERNEL_OK,
      "seed keep note should succeed");
  require_true(
      kernel_write_note(handle, "delete.md", delete_original.data(), delete_original.size(), nullptr, &metadata, &disposition).code == KERNEL_OK,
      "seed delete note should succeed");
  require_true(kernel_close(handle).code == KERNEL_OK, "vault should close");

  write_file_bytes(
      root / "keep.md",
      "# Keep After\n"
      "keep-after-token\n");
  std::filesystem::remove(root / "delete.md");
  write_file_bytes(
      root / "create.md",
      "# Create After\n"
      "create-after-token\n");

  auto db = open_search_db(root);
  const std::vector<CoalescedAction> actions = {
      {CoalescedActionKind::FullRescan, ""}};
  const std::error_code ec = kernel::watcher::apply_actions(db, root, actions);
  require_true(!ec, "full rescan action should succeed");

  std::vector<kernel::search::SearchHit> hits;
  require_true(!kernel::search::search_notes(db, "keep-before-token", hits), "old keep token search should succeed");
  require_true(hits.empty(), "full rescan should remove stale modified content");
  require_true(!kernel::search::search_notes(db, "keep-after-token", hits), "new keep token search should succeed");
  require_true(hits.size() == 1 && hits[0].rel_path == "keep.md", "full rescan should reindex modified note");

  require_true(!kernel::search::search_notes(db, "delete-before-token", hits), "deleted token search should succeed");
  require_true(hits.empty(), "full rescan should remove deleted note from search");
  require_true(!kernel::search::search_notes(db, "create-after-token", hits), "created token search should succeed");
  require_true(hits.size() == 1 && hits[0].rel_path == "create.md", "full rescan should index new note");

  sqlite3* readonly_db = nullptr;
  require_true(
      sqlite3_open_v2(storage_db_for_vault(root).string().c_str(), &readonly_db, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK,
      "readonly sqlite db should open");
  require_true(
      query_single_int(readonly_db, "SELECT COUNT(*) FROM notes WHERE rel_path='delete.md' AND is_deleted=1;") == 1,
      "full rescan should mark missing note deleted");
  require_true(
      query_single_int(readonly_db, "SELECT COUNT(*) FROM notes WHERE rel_path='create.md' AND is_deleted=0;") == 1,
      "full rescan should add created note");
  sqlite3_close(readonly_db);

  kernel::storage::close(db);
  std::filesystem::remove_all(root);
  std::filesystem::remove_all(state_dir_for_vault(root));
}

}  // namespace

void run_watcher_refresh_integration_tests() {
  test_poll_and_refresh_once_indexes_external_create();
  test_poll_and_refresh_once_updates_external_modify();
  test_poll_and_refresh_once_applies_external_delete();
  test_apply_actions_full_rescan_reconciles_mixed_changes();
}
