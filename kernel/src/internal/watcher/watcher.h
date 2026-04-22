// Reason: This file defines the narrow raw-event and coalesced-action model that a future Windows watcher will feed.

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace kernel::watcher {

enum class RawChangeKind {
  Created,
  Modified,
  Deleted,
  RenamedOld,
  RenamedNew,
  Overflow
};

struct RawChangeEvent {
  RawChangeKind kind;
  std::string rel_path;
};

enum class CoalescedActionKind {
  RefreshPath,
  DeletePath,
  RenamePath,
  FullRescan
};

struct CoalescedAction {
  CoalescedActionKind kind;
  std::string rel_path;
  std::string secondary_rel_path;
  std::string continuity_fallback_reason;
};

std::vector<CoalescedAction> coalesce_events(const std::vector<RawChangeEvent>& events);

}  // namespace kernel::watcher
