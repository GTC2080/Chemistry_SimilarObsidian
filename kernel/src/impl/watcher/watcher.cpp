// Reason: This file implements the minimal event coalescing rules that keep watcher-driven refresh work predictable.

#include "watcher/watcher.h"

#include <algorithm>
#include <deque>
#include <unordered_map>

namespace kernel::watcher {
namespace {

enum class PendingKind {
  None,
  CreatedPending,
  Refresh,
  Delete
};

struct OrderedEntry {
  std::size_t order = 0;
};

struct PendingEntry : OrderedEntry {
  PendingKind kind = PendingKind::None;
};

struct DirectEntry : OrderedEntry {
  CoalescedAction action;
};

struct PendingRenameOld : OrderedEntry {
  std::string rel_path;
};

void consume_following_refresh_noise(
    const std::vector<RawChangeEvent>& events,
    const std::string& rel_path,
    const std::size_t start_index,
    std::vector<bool>& consumed) {
  for (std::size_t index = start_index; index < events.size(); ++index) {
    if (consumed[index] || events[index].rel_path != rel_path) {
      continue;
    }
    if (events[index].kind != RawChangeKind::Created &&
        events[index].kind != RawChangeKind::Modified) {
      continue;
    }
    consumed[index] = true;
  }
}

PendingKind apply_event(PendingKind current, RawChangeKind incoming) {
  switch (incoming) {
    case RawChangeKind::Created:
      if (current == PendingKind::None) {
        return PendingKind::CreatedPending;
      }
      return PendingKind::Refresh;

    case RawChangeKind::Modified:
      return PendingKind::Refresh;

    case RawChangeKind::Deleted:
    case RawChangeKind::RenamedOld:
      if (current == PendingKind::CreatedPending) {
        return PendingKind::None;
      }
      return PendingKind::Delete;

    case RawChangeKind::RenamedNew:
      if (current == PendingKind::None) {
        return PendingKind::CreatedPending;
      }
      return PendingKind::Refresh;

    case RawChangeKind::Overflow:
      return current;
  }

  return current;
}

bool has_hidden_path_segment(std::string_view rel_path) {
  std::size_t start = 0;
  while (start <= rel_path.size()) {
    const std::size_t next = rel_path.find('/', start);
    const std::string_view segment =
        next == std::string_view::npos ? rel_path.substr(start) : rel_path.substr(start, next - start);
    if (!segment.empty() && segment.front() == '.') {
      return true;
    }
    if (next == std::string_view::npos) {
      break;
    }
    start = next + 1;
  }
  return false;
}

}  // namespace

std::vector<CoalescedAction> coalesce_events(const std::vector<RawChangeEvent>& events) {
  for (const auto& event : events) {
    if (event.kind == RawChangeKind::Overflow) {
      return {{CoalescedActionKind::FullRescan, "", ""}};
    }
  }

  std::vector<RawChangeEvent> visible_events;
  visible_events.reserve(events.size());
  for (const auto& event : events) {
    if (!has_hidden_path_segment(event.rel_path)) {
      visible_events.push_back(event);
    }
  }
  if (visible_events.empty()) {
    return {};
  }

  std::unordered_map<std::string, PendingEntry> pending;
  std::vector<std::string> order;
  std::vector<DirectEntry> direct_entries;
  std::vector<bool> consumed(visible_events.size(), false);
  std::deque<PendingRenameOld> pending_rename_old;

  for (std::size_t index = 0; index < visible_events.size(); ++index) {
    const auto& event = visible_events[index];
    if (event.kind == RawChangeKind::RenamedOld) {
      pending_rename_old.push_back(PendingRenameOld{index, event.rel_path});
      continue;
    }
    if (event.kind != RawChangeKind::RenamedNew || pending_rename_old.empty()) {
      continue;
    }

    const PendingRenameOld old = pending_rename_old.front();
    pending_rename_old.pop_front();
    direct_entries.push_back(
        DirectEntry{
            old.order,
            CoalescedAction{
                CoalescedActionKind::RenamePath,
                old.rel_path,
                event.rel_path,
                ""}});
    consumed[old.order] = true;
    consumed[index] = true;
    consume_following_refresh_noise(visible_events, event.rel_path, index + 1, consumed);
  }

  for (std::size_t index = 0; index < visible_events.size(); ++index) {
    if (consumed[index]) {
      continue;
    }

    const auto& event = visible_events[index];
    if (event.kind == RawChangeKind::RenamedOld) {
      direct_entries.push_back(
          DirectEntry{
              index,
              CoalescedAction{
                  CoalescedActionKind::DeletePath,
                  event.rel_path,
                  "",
                  "rename_old_without_new"}});
      consumed[index] = true;
      continue;
    }
    if (event.kind == RawChangeKind::RenamedNew) {
      direct_entries.push_back(
          DirectEntry{
              index,
              CoalescedAction{
                  CoalescedActionKind::RefreshPath,
                  event.rel_path,
                  "",
                  "rename_new_without_old"}});
      consumed[index] = true;
      consume_following_refresh_noise(visible_events, event.rel_path, index + 1, consumed);
      continue;
    }

    auto [it, inserted] = pending.try_emplace(
        event.rel_path,
        PendingEntry{order.size(), PendingKind::None});
    if (inserted) {
      order.push_back(event.rel_path);
    }
    it->second.kind = apply_event(it->second.kind, event.kind);
  }

  std::vector<DirectEntry> ordered_entries = std::move(direct_entries);
  ordered_entries.reserve(ordered_entries.size() + order.size());

  for (const auto& rel_path : order) {
    const auto it = pending.find(rel_path);
    if (it == pending.end()) {
      continue;
    }

    switch (it->second.kind) {
      case PendingKind::None:
        break;
      case PendingKind::CreatedPending:
      case PendingKind::Refresh:
        ordered_entries.push_back(
            DirectEntry{it->second.order, CoalescedAction{CoalescedActionKind::RefreshPath, rel_path, ""}});
        break;
      case PendingKind::Delete:
        ordered_entries.push_back(
            DirectEntry{it->second.order, CoalescedAction{CoalescedActionKind::DeletePath, rel_path, ""}});
        break;
    }
  }

  std::sort(
      ordered_entries.begin(),
      ordered_entries.end(),
      [](const DirectEntry& left, const DirectEntry& right) {
        return left.order < right.order;
      });

  std::vector<CoalescedAction> actions;
  actions.reserve(ordered_entries.size());
  for (auto& entry : ordered_entries) {
    actions.push_back(std::move(entry.action));
  }

  return actions;
}

}  // namespace kernel::watcher
