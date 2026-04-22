// Reason: Keep watcher event coalescing and Win32 decode regressions together because they lock the raw-event to action contract.

#include "watcher/watcher_test_suites.h"

#include "watcher/watcher.h"
#include "watcher/windows_decode.h"
#include "support/test_support.h"

#include <Windows.h>

#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

namespace {

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

}  // namespace

void run_watcher_event_coalescing_tests() {
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
}
