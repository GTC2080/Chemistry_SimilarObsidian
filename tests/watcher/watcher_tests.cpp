// Reason: This file locks the first watcher event coalescing rules before any live Win32 watcher loop is introduced.

#include "watcher/watcher.h"
#include "watcher/integration.h"
#include "watcher/session.h"
#include "watcher/windows_decode.h"
#include "index/refresh.h"
#include "kernel/c_api.h"
#include "search/search.h"
#include "storage/storage.h"
#include "support/test_support.h"
#include "third_party/sqlite/sqlite3.h"

#include <Windows.h>

#include <cstddef>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using kernel::watcher::CoalescedAction;
using kernel::watcher::CoalescedActionKind;
using kernel::watcher::RawChangeEvent;
using kernel::watcher::RawChangeKind;

std::vector<std::byte> make_notify_buffer(
    DWORD action,
    const std::wstring& file_name,
    DWORD next_entry_offset) {
  std::vector<std::byte> bytes(sizeof(FILE_NOTIFY_INFORMATION) + file_name.size() * sizeof(wchar_t));
  auto* record = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(bytes.data());
  record->NextEntryOffset = next_entry_offset;
  record->Action = action;
  record->FileNameLength = static_cast<DWORD>(file_name.size() * sizeof(wchar_t));
  std::memcpy(record->FileName, file_name.data(), record->FileNameLength);
  return bytes;
}

std::vector<std::byte> concat_buffers(std::vector<std::byte> first, const std::vector<std::byte>& second) {
  const auto first_size = first.size();
  first.resize(first_size + second.size());
  std::memcpy(first.data() + first_size, second.data(), second.size());
  return first;
}

void test_create_and_modify_coalesce_to_one_refresh() {
  const std::vector<RawChangeEvent> events = {
      {RawChangeKind::Created, "note.md"},
      {RawChangeKind::Modified, "note.md"}};

  const auto actions = kernel::watcher::coalesce_events(events);
  require_true(actions.size() == 1, "create+modify should produce one action");
  require_true(actions[0].kind == CoalescedActionKind::RefreshPath, "create+modify should coalesce to refresh");
  require_true(actions[0].rel_path == "note.md", "coalesced refresh should preserve path");
}

void test_create_then_delete_cancels_the_path() {
  const std::vector<RawChangeEvent> events = {
      {RawChangeKind::Created, "note.md"},
      {RawChangeKind::Deleted, "note.md"}};

  const auto actions = kernel::watcher::coalesce_events(events);
  require_true(actions.empty(), "create+delete in one window should cancel out");
}

void test_modify_then_delete_coalesces_to_delete() {
  const std::vector<RawChangeEvent> events = {
      {RawChangeKind::Modified, "note.md"},
      {RawChangeKind::Deleted, "note.md"}};

  const auto actions = kernel::watcher::coalesce_events(events);
  require_true(actions.size() == 1, "modify+delete should produce one action");
  require_true(actions[0].kind == CoalescedActionKind::DeletePath, "modify+delete should coalesce to delete");
  require_true(actions[0].rel_path == "note.md", "coalesced delete should preserve path");
}

void test_delete_then_create_coalesces_to_refresh() {
  const std::vector<RawChangeEvent> events = {
      {RawChangeKind::Deleted, "note.md"},
      {RawChangeKind::Created, "note.md"}};

  const auto actions = kernel::watcher::coalesce_events(events);
  require_true(actions.size() == 1, "delete+create should produce one action");
  require_true(actions[0].kind == CoalescedActionKind::RefreshPath, "delete+create should coalesce to refresh");
  require_true(actions[0].rel_path == "note.md", "recreated path should preserve rel_path");
}

void test_rename_old_and_new_coalesce_to_rename_action() {
  const std::vector<RawChangeEvent> events = {
      {RawChangeKind::RenamedOld, "old-name.md"},
      {RawChangeKind::RenamedNew, "new-name.md"}};

  const auto actions = kernel::watcher::coalesce_events(events);
  require_true(actions.size() == 1, "paired rename events should produce one action");
  require_true(actions[0].kind == CoalescedActionKind::RenamePath, "paired rename events should coalesce to rename");
  require_true(actions[0].rel_path == "old-name.md", "rename action should preserve old path");
  require_true(actions[0].secondary_rel_path == "new-name.md", "rename action should preserve new path");
  require_true(
      actions[0].continuity_fallback_reason.empty(),
      "recognized rename continuity should not carry a fallback reason");
}

void test_move_old_and_new_paths_coalesce_to_rename_action() {
  const std::vector<RawChangeEvent> events = {
      {RawChangeKind::RenamedOld, "from/move-me.md"},
      {RawChangeKind::RenamedNew, "to/move-me.md"}};

  const auto actions = kernel::watcher::coalesce_events(events);
  require_true(actions.size() == 1, "paired move events should produce one action");
  require_true(actions[0].kind == CoalescedActionKind::RenamePath, "paired move events should coalesce to rename");
  require_true(actions[0].rel_path == "from/move-me.md", "move action should preserve old path");
  require_true(actions[0].secondary_rel_path == "to/move-me.md", "move action should preserve new path");
  require_true(
      actions[0].continuity_fallback_reason.empty(),
      "recognized move continuity should not carry a fallback reason");
}

void test_rename_pair_followed_by_new_path_modify_stays_one_rename_action() {
  const std::vector<RawChangeEvent> events = {
      {RawChangeKind::RenamedOld, "old-name.md"},
      {RawChangeKind::RenamedNew, "new-name.md"},
      {RawChangeKind::Modified, "new-name.md"}};

  const auto actions = kernel::watcher::coalesce_events(events);
  require_true(actions.size() == 1, "rename plus new-path modify should still produce one action");
  require_true(actions[0].kind == CoalescedActionKind::RenamePath, "rename plus new-path modify should keep continuity");
  require_true(actions[0].rel_path == "old-name.md", "rename+modify continuity should preserve old path");
  require_true(actions[0].secondary_rel_path == "new-name.md", "rename+modify continuity should preserve new path");
  require_true(
      actions[0].continuity_fallback_reason.empty(),
      "rename+modify continuity should not degrade to a fallback path");
}

void test_unpaired_rename_old_falls_back_to_delete() {
  const std::vector<RawChangeEvent> events = {
      {RawChangeKind::RenamedOld, "old-name.md"}};

  const auto actions = kernel::watcher::coalesce_events(events);
  require_true(actions.size() == 1, "unpaired rename-old should produce one action");
  require_true(actions[0].kind == CoalescedActionKind::DeletePath, "unpaired rename-old should fall back to delete");
  require_true(actions[0].rel_path == "old-name.md", "fallback delete should preserve old path");
  require_true(
      actions[0].continuity_fallback_reason == "rename_old_without_new",
      "unpaired rename-old should carry a stable continuity fallback reason");
}

void test_unpaired_rename_new_falls_back_to_refresh_with_reason() {
  const std::vector<RawChangeEvent> events = {
      {RawChangeKind::RenamedNew, "new-name.md"}};

  const auto actions = kernel::watcher::coalesce_events(events);
  require_true(actions.size() == 1, "unpaired rename-new should produce one action");
  require_true(actions[0].kind == CoalescedActionKind::RefreshPath, "unpaired rename-new should fall back to refresh");
  require_true(actions[0].rel_path == "new-name.md", "fallback refresh should preserve new path");
  require_true(
      actions[0].continuity_fallback_reason == "rename_new_without_old",
      "unpaired rename-new should carry a stable continuity fallback reason");
}

void test_overflow_forces_full_rescan() {
  const std::vector<RawChangeEvent> events = {
      {RawChangeKind::Modified, "a.md"},
      {RawChangeKind::Overflow, ""},
      {RawChangeKind::Deleted, "b.md"}};

  const auto actions = kernel::watcher::coalesce_events(events);
  require_true(actions.size() == 1, "overflow should collapse to one action");
  require_true(actions[0].kind == CoalescedActionKind::FullRescan, "overflow should force full rescan");
  require_true(actions[0].rel_path.empty(), "full rescan should not carry a path");
}

void test_actions_preserve_first_path_order() {
  const std::vector<RawChangeEvent> events = {
      {RawChangeKind::Modified, "b.md"},
      {RawChangeKind::Modified, "a.md"}};

  const auto actions = kernel::watcher::coalesce_events(events);
  require_true(actions.size() == 2, "two independent paths should produce two actions");
  require_true(actions[0].rel_path == "b.md", "coalesced actions should preserve first-seen path order");
  require_true(actions[1].rel_path == "a.md", "coalesced actions should preserve first-seen path order");
}

void test_decode_win32_added_and_modified_records() {
  auto first = make_notify_buffer(FILE_ACTION_ADDED, L"alpha.md", 0);
  auto second = make_notify_buffer(FILE_ACTION_MODIFIED, L"beta.md", 0);
  reinterpret_cast<FILE_NOTIFY_INFORMATION*>(first.data())->NextEntryOffset = static_cast<DWORD>(first.size());
  auto bytes = concat_buffers(std::move(first), second);

  const auto events = kernel::watcher::decode_win32_notify_buffer(bytes.data(), bytes.size());
  require_true(events.size() == 2, "two notify records should decode to two events");
  require_true(events[0].kind == RawChangeKind::Created, "FILE_ACTION_ADDED should decode to Created");
  require_true(events[0].rel_path == "alpha.md", "decoded created event should preserve path");
  require_true(events[1].kind == RawChangeKind::Modified, "FILE_ACTION_MODIFIED should decode to Modified");
  require_true(events[1].rel_path == "beta.md", "decoded modified event should preserve path");
}

void test_decode_win32_removed_and_rename_records() {
  auto removed = make_notify_buffer(FILE_ACTION_REMOVED, L"old.md", 0);
  auto renamed_new = make_notify_buffer(FILE_ACTION_RENAMED_NEW_NAME, L"new.md", 0);
  reinterpret_cast<FILE_NOTIFY_INFORMATION*>(removed.data())->NextEntryOffset = static_cast<DWORD>(removed.size());
  auto bytes = concat_buffers(std::move(removed), renamed_new);

  const auto events = kernel::watcher::decode_win32_notify_buffer(bytes.data(), bytes.size());
  require_true(events.size() == 2, "removed and renamed-new records should decode to two events");
  require_true(events[0].kind == RawChangeKind::Deleted, "FILE_ACTION_REMOVED should decode to Deleted");
  require_true(events[0].rel_path == "old.md", "decoded deleted event should preserve path");
  require_true(events[1].kind == RawChangeKind::RenamedNew, "renamed new name should decode to RenamedNew");
  require_true(events[1].rel_path == "new.md", "decoded rename-new event should preserve path");
}

void test_decode_win32_rename_old_name_becomes_renamed_old() {
  const auto bytes = make_notify_buffer(FILE_ACTION_RENAMED_OLD_NAME, L"moved-old.md", 0);
  const auto events = kernel::watcher::decode_win32_notify_buffer(bytes.data(), bytes.size());
  require_true(events.size() == 1, "single rename-old record should decode to one event");
  require_true(events[0].kind == RawChangeKind::RenamedOld, "renamed old name should decode to RenamedOld");
  require_true(events[0].rel_path == "moved-old.md", "decoded rename-old event should preserve path");
}

void test_make_overflow_event() {
  const auto event = kernel::watcher::make_overflow_event();
  require_true(event.kind == RawChangeKind::Overflow, "overflow helper should produce Overflow event");
  require_true(event.rel_path.empty(), "overflow helper should not carry a path");
}

std::filesystem::path make_temp_watch_root() {
  return make_temp_vault("chem_kernel_watcher_test_");
}

kernel::storage::Database open_search_db(const std::filesystem::path& vault) {
  kernel::storage::Database db;
  const std::error_code ec = kernel::storage::open_or_create(storage_db_for_vault(vault), db);
  require_true(!ec, "watcher test search db should open");
  return db;
}

void test_watch_session_open_and_close() {
  const auto root = make_temp_watch_root();
  kernel::watcher::WatchSession session;
  const std::error_code ec = kernel::watcher::open_watch_session(root, session);
  require_true(!ec, "watch session should open");
  require_true(session.directory_handle != INVALID_HANDLE_VALUE, "watch session should own a directory handle");
  require_true(session.event_handle != nullptr, "watch session should own an event handle");
  kernel::watcher::close_watch_session(session);
  require_true(session.directory_handle == INVALID_HANDLE_VALUE, "close should release directory handle");
  require_true(session.event_handle == nullptr, "close should release event handle");
  std::filesystem::remove_all(root);
}

void test_watch_session_poll_once_reports_create_as_refresh() {
  const auto root = make_temp_watch_root();
  kernel::watcher::WatchSession session;
  require_true(!kernel::watcher::open_watch_session(root, session), "watch session should open");

  write_file_bytes(root / "created.md", "created");

  std::vector<CoalescedAction> actions;
  const std::error_code ec = kernel::watcher::poll_watch_session_once(session, 1000, actions);
  require_true(!ec, "create poll should succeed");
  require_true(actions.size() == 1, "create poll should yield one action");
  require_true(actions[0].kind == CoalescedActionKind::RefreshPath, "create poll should coalesce to refresh");
  require_true(actions[0].rel_path == "created.md", "create poll should preserve path");

  kernel::watcher::close_watch_session(session);
  std::filesystem::remove_all(root);
}

void test_watch_session_poll_once_reports_injected_overflow_as_full_rescan() {
  const auto root = make_temp_watch_root();
  kernel::watcher::WatchSession session;
  require_true(!kernel::watcher::open_watch_session(root, session), "watch session should open");

  kernel::watcher::inject_next_poll_overflow(session);

  std::vector<CoalescedAction> actions;
  const std::error_code ec = kernel::watcher::poll_watch_session_once(session, 1000, actions);
  require_true(!ec, "injected overflow poll should succeed");
  require_true(actions.size() == 1, "injected overflow poll should yield one action");
  require_true(
      actions[0].kind == CoalescedActionKind::FullRescan,
      "injected overflow poll should surface a full-rescan action");
  require_true(actions[0].rel_path.empty(), "injected overflow full-rescan should not carry a path");

  kernel::watcher::close_watch_session(session);
  std::filesystem::remove_all(root);
}

void test_watch_session_poll_once_reports_modify_as_refresh() {
  const auto root = make_temp_watch_root();
  write_file_bytes(root / "modified.md", "before");

  kernel::watcher::WatchSession session;
  require_true(!kernel::watcher::open_watch_session(root, session), "watch session should open");

  write_file_bytes(root / "modified.md", "after");

  std::vector<CoalescedAction> actions;
  const std::error_code ec = kernel::watcher::poll_watch_session_once(session, 1000, actions);
  require_true(!ec, "modify poll should succeed");
  require_true(actions.size() == 1, "modify poll should yield one action");
  require_true(actions[0].kind == CoalescedActionKind::RefreshPath, "modify poll should coalesce to refresh");
  require_true(actions[0].rel_path == "modified.md", "modify poll should preserve path");

  kernel::watcher::close_watch_session(session);
  std::filesystem::remove_all(root);
}

void test_watch_session_poll_once_reports_delete_as_delete() {
  const auto root = make_temp_watch_root();
  write_file_bytes(root / "deleted.md", "before-delete");

  kernel::watcher::WatchSession session;
  require_true(!kernel::watcher::open_watch_session(root, session), "watch session should open");

  std::filesystem::remove(root / "deleted.md");

  std::vector<CoalescedAction> actions;
  const std::error_code ec = kernel::watcher::poll_watch_session_once(session, 1000, actions);
  require_true(!ec, "delete poll should succeed");
  require_true(actions.size() == 1, "delete poll should yield one action");
  require_true(actions[0].kind == CoalescedActionKind::DeletePath, "delete poll should coalesce to delete");
  require_true(actions[0].rel_path == "deleted.md", "delete poll should preserve path");

  kernel::watcher::close_watch_session(session);
  std::filesystem::remove_all(root);
}

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

  sqlite3* readonly_db = nullptr;
  require_true(
      sqlite3_open_v2(storage_db_for_vault(root).string().c_str(), &readonly_db, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK,
      "readonly sqlite db should open");
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

int main() {
  try {
    test_create_and_modify_coalesce_to_one_refresh();
    test_create_then_delete_cancels_the_path();
    test_modify_then_delete_coalesces_to_delete();
    test_delete_then_create_coalesces_to_refresh();
    test_rename_old_and_new_coalesce_to_rename_action();
    test_move_old_and_new_paths_coalesce_to_rename_action();
    test_rename_pair_followed_by_new_path_modify_stays_one_rename_action();
    test_unpaired_rename_old_falls_back_to_delete();
    test_unpaired_rename_new_falls_back_to_refresh_with_reason();
    test_overflow_forces_full_rescan();
    test_actions_preserve_first_path_order();
    test_decode_win32_added_and_modified_records();
    test_decode_win32_removed_and_rename_records();
    test_decode_win32_rename_old_name_becomes_renamed_old();
    test_make_overflow_event();
    test_watch_session_open_and_close();
    test_watch_session_poll_once_reports_create_as_refresh();
    test_watch_session_poll_once_reports_injected_overflow_as_full_rescan();
    test_watch_session_poll_once_reports_modify_as_refresh();
    test_watch_session_poll_once_reports_delete_as_delete();
    test_poll_and_refresh_once_indexes_external_create();
    test_poll_and_refresh_once_updates_external_modify();
    test_poll_and_refresh_once_applies_external_delete();
    test_poll_and_refresh_once_preserves_note_identity_across_external_rename();
    test_apply_actions_preserves_note_identity_across_move();
    test_apply_actions_reconciles_attachment_rename();
    test_apply_actions_full_rescan_reconciles_mixed_changes();
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "watcher_tests failed: " << ex.what() << "\n";
    return 1;
  }
}
