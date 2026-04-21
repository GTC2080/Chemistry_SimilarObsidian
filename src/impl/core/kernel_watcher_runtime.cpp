// Reason: This file owns the watcher runtime loop, suppression filtering, and retry timing.

#include "core/kernel_runtime_internal.h"

#include "core/kernel_shared.h"
#include "index/refresh.h"
#include "platform/platform.h"
#include "vault/revision.h"
#include "watcher/integration.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <thread>
#include <vector>

namespace kernel::core {

namespace {

constexpr auto kWatcherFailureRetryBackoff = std::chrono::milliseconds(50);

void prune_expired_suppressed_paths(
    kernel_handle* handle,
    const std::chrono::steady_clock::time_point now) {
  for (auto it = handle->runtime.suppressed_watcher_paths.begin();
       it != handle->runtime.suppressed_watcher_paths.end();) {
    if (it->second.expires_at <= now) {
      it = handle->runtime.suppressed_watcher_paths.erase(it);
    } else {
      ++it;
    }
  }
}

std::vector<kernel::watcher::CoalescedAction> filter_suppressed_actions(
    kernel_handle* handle,
    const std::vector<kernel::watcher::CoalescedAction>& actions) {
  const auto now = std::chrono::steady_clock::now();
  prune_expired_suppressed_paths(handle, now);

  std::vector<kernel::watcher::CoalescedAction> filtered;
  filtered.reserve(actions.size());
  for (const auto& action : actions) {
    if (action.kind == kernel::watcher::CoalescedActionKind::FullRescan) {
      filtered.push_back(action);
      continue;
    }

    const std::string normalized_rel_path = normalize_rel_path(action.rel_path);
    const auto it = handle->runtime.suppressed_watcher_paths.find(normalized_rel_path);
    if (it == handle->runtime.suppressed_watcher_paths.end()) {
      filtered.push_back(action);
      continue;
    }

    const auto target_path =
        (handle->paths.root / std::filesystem::path(normalized_rel_path)).lexically_normal();
    kernel::platform::ReadFileResult file;
    const std::error_code read_ec = kernel::platform::read_file(target_path, file);
    if (!read_ec &&
        kernel::vault::compute_content_revision(file.bytes) == it->second.content_revision) {
      continue;
    }

    handle->runtime.suppressed_watcher_paths.erase(it);
    filtered.push_back(action);
  }

  return filtered;
}

}  // namespace

void sleep_with_stop(const std::stop_token stop_token, const std::chrono::milliseconds duration) {
  auto remaining = duration;
  while (remaining.count() > 0 && !stop_token.stop_requested()) {
    const auto chunk = std::min(remaining, std::chrono::milliseconds(10));
    std::this_thread::sleep_for(chunk);
    remaining -= chunk;
  }
}

void watcher_loop(std::stop_token stop_token, kernel_handle* handle) {
  while (!stop_token.stop_requested()) {
    if (rebuild_in_progress(handle)) {
      sleep_with_stop(stop_token, std::chrono::milliseconds(10));
      continue;
    }

    bool delay_after_failed_catch_up = false;
    bool handled_initial_catch_up = false;
    {
      std::lock_guard lock(handle->storage_mutex);
      if (handle->storage.connection == nullptr) {
        return;
      }
      if (!handle->runtime.initial_catch_up_complete) {
        handled_initial_catch_up = true;
        const std::error_code catch_up_ec =
            kernel::index::full_rescan_markdown_vault(handle->storage, handle->paths.root, stop_token);
        if (catch_up_ec) {
          if (catch_up_ec == std::make_error_code(std::errc::operation_canceled) &&
              stop_token.stop_requested()) {
            return;
          }
          {
            std::lock_guard runtime_lock(handle->runtime_mutex);
            handle->runtime.index_state = KERNEL_INDEX_UNAVAILABLE;
          }
          set_index_fault(handle, "initial_catch_up_failed", catch_up_ec.value());
          delay_after_failed_catch_up = true;
        } else {
          std::uint64_t indexed_note_count = 0;
          const std::error_code count_ec =
              kernel::storage::count_active_notes(handle->storage, indexed_note_count);
          if (count_ec) {
            {
              std::lock_guard runtime_lock(handle->runtime_mutex);
              handle->runtime.index_state = KERNEL_INDEX_UNAVAILABLE;
            }
            set_index_fault(handle, "initial_catch_up_failed", count_ec.value());
            delay_after_failed_catch_up = true;
            continue;
          }
          {
            std::lock_guard runtime_lock(handle->runtime_mutex);
            handle->runtime.initial_catch_up_complete = true;
            handle->runtime.index_state = KERNEL_INDEX_READY;
            handle->runtime.indexed_note_count = indexed_note_count;
          }
          clear_index_fault(handle);
        }
      }
    }

    if (handled_initial_catch_up) {
      if (delay_after_failed_catch_up) {
        sleep_with_stop(stop_token, std::chrono::milliseconds(100));
      }
      continue;
    }

    std::vector<kernel::watcher::CoalescedAction> actions;
    const std::error_code poll_ec =
        kernel::watcher::poll_watch_session_once(handle->watcher_session, 100, actions);
    if (rebuild_in_progress(handle)) {
      continue;
    }
    if (poll_ec) {
      {
        std::lock_guard runtime_lock(handle->runtime_mutex);
        handle->runtime.index_state = KERNEL_INDEX_UNAVAILABLE;
      }
      set_index_fault(handle, "watcher_poll_failed", poll_ec.value());
      sleep_with_stop(stop_token, kWatcherFailureRetryBackoff);
      continue;
    }

    std::string recovered_fault_reason;
    {
      std::lock_guard runtime_lock(handle->runtime_mutex);
      recovered_fault_reason = handle->runtime.index_fault.reason;
      handle->runtime.index_state = KERNEL_INDEX_READY;
    }
    if (!recovered_fault_reason.empty()) {
      record_recent_event(handle, "watcher_recovered", recovered_fault_reason, 0);
    }
    clear_index_fault(handle);

    if (actions.empty()) {
      continue;
    }

    std::lock_guard lock(handle->storage_mutex);
    if (handle->storage.connection == nullptr) {
      return;
    }
    actions = filter_suppressed_actions(handle, actions);
    if (actions.empty()) {
      continue;
    }
    for (const auto& action : actions) {
      if (!action.continuity_fallback_reason.empty()) {
        record_continuity_fallback(handle, action.continuity_fallback_reason);
      }
    }
    const std::error_code apply_ec =
        kernel::watcher::apply_actions(handle->storage, handle->paths.root, actions, stop_token);
    if (rebuild_in_progress(handle)) {
      continue;
    }
    if (apply_ec == std::make_error_code(std::errc::operation_canceled) &&
        stop_token.stop_requested()) {
      return;
    }
    if (apply_ec) {
      {
        std::lock_guard runtime_lock(handle->runtime_mutex);
        handle->runtime.index_state = KERNEL_INDEX_UNAVAILABLE;
      }
      set_index_fault(handle, "watcher_apply_failed", apply_ec.value());
      sleep_with_stop(stop_token, kWatcherFailureRetryBackoff);
      continue;
    }

    std::uint64_t indexed_note_count = 0;
    const std::error_code count_ec =
        kernel::storage::count_active_notes(handle->storage, indexed_note_count);
    if (rebuild_in_progress(handle)) {
      continue;
    }
    if (count_ec) {
      {
        std::lock_guard runtime_lock(handle->runtime_mutex);
        handle->runtime.index_state = KERNEL_INDEX_UNAVAILABLE;
      }
      set_index_fault(handle, "watcher_apply_failed", count_ec.value());
      sleep_with_stop(stop_token, kWatcherFailureRetryBackoff);
      continue;
    }
    {
      std::lock_guard runtime_lock(handle->runtime_mutex);
      handle->runtime.indexed_note_count = indexed_note_count;
    }
  }
}

}  // namespace kernel::core
