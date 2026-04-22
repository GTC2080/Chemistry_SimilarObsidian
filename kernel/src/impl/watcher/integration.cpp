// Reason: This file implements the minimal watcher-to-refresh bridge before any background watcher runtime is introduced.

#include "watcher/integration.h"

#include "index/refresh.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>

namespace kernel::watcher {
namespace {

std::atomic<int> g_apply_delay_after_count{0};
std::atomic<int> g_apply_delay_ms{0};
std::atomic<int> g_apply_delay_remaining{0};

bool stop_requested(const std::stop_token stop_token) {
  return stop_token.stop_possible() && stop_token.stop_requested();
}

std::error_code maybe_delay_after_action(
    const std::stop_token stop_token,
    const int applied_action_count) {
  if (applied_action_count != g_apply_delay_after_count.load(std::memory_order_relaxed)) {
    return {};
  }

  int remaining_delays = g_apply_delay_remaining.load(std::memory_order_relaxed);
  while (remaining_delays > 0) {
    if (g_apply_delay_remaining.compare_exchange_weak(
            remaining_delays,
            remaining_delays - 1,
            std::memory_order_relaxed)) {
      int remaining_delay_ms = g_apply_delay_ms.load(std::memory_order_relaxed);
      while (remaining_delay_ms > 0) {
        if (stop_requested(stop_token)) {
          return std::make_error_code(std::errc::operation_canceled);
        }
        const int sleep_ms = std::min(remaining_delay_ms, 10);
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
        remaining_delay_ms -= sleep_ms;
      }
      break;
    }
  }

  if (stop_requested(stop_token)) {
    return std::make_error_code(std::errc::operation_canceled);
  }

  return {};
}

}  // namespace

void inject_apply_actions_delay_after_count(
    const int after_action_count,
    const int delay_ms,
    const int remaining_delays) {
  g_apply_delay_after_count.store(
      after_action_count > 0 ? after_action_count : 0,
      std::memory_order_relaxed);
  g_apply_delay_ms.store(delay_ms > 0 ? delay_ms : 0, std::memory_order_relaxed);
  g_apply_delay_remaining.store(
      remaining_delays > 0 ? remaining_delays : 0,
      std::memory_order_relaxed);
}

std::error_code apply_actions(
    kernel::storage::Database& db,
    const std::filesystem::path& vault_root,
    const std::vector<CoalescedAction>& actions,
    const std::stop_token stop_token) {
  std::error_code ec;
  int applied_action_count = 0;
  for (const auto& action : actions) {
    if (stop_requested(stop_token)) {
      return std::make_error_code(std::errc::operation_canceled);
    }

    switch (action.kind) {
      case CoalescedActionKind::RefreshPath:
      case CoalescedActionKind::DeletePath:
        ec = kernel::index::refresh_markdown_path(db, vault_root, action.rel_path);
        if (ec) {
          return ec;
        }
        break;
      case CoalescedActionKind::RenamePath:
        ec = kernel::index::rename_or_refresh_path(
            db,
            vault_root,
            action.rel_path,
            action.secondary_rel_path);
        if (ec) {
          return ec;
        }
        break;
      case CoalescedActionKind::FullRescan:
        ec = kernel::index::full_rescan_markdown_vault(db, vault_root);
        if (ec) {
          return ec;
        }
        break;
    }

    ++applied_action_count;
    ec = maybe_delay_after_action(stop_token, applied_action_count);
    if (ec) {
      return ec;
    }
  }

  return {};
}

std::error_code poll_and_refresh_once(
    WatchSession& session,
    kernel::storage::Database& db,
    const DWORD timeout_ms,
    std::vector<CoalescedAction>& out_actions,
    const std::stop_token stop_token) {
  std::error_code ec = poll_watch_session_once(session, timeout_ms, out_actions);
  if (ec) {
    return ec;
  }
  return apply_actions(db, session.root, out_actions, stop_token);
}

}  // namespace kernel::watcher
