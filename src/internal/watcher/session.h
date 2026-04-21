// Reason: This file defines the minimal Windows watcher session skeleton before any long-running loop is introduced.

#pragma once

#include "watcher/watcher.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <atomic>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <system_error>
#include <vector>

namespace kernel::watcher {

struct WatchSession {
  HANDLE directory_handle = INVALID_HANDLE_VALUE;
  HANDLE event_handle = nullptr;
  std::filesystem::path root;
  std::vector<std::byte> buffer;
  OVERLAPPED overlapped{};
  bool watch_armed = false;
  std::atomic<int> injected_poll_error_value = 0;
  std::atomic<int> injected_poll_error_remaining = 0;
  std::atomic<int> injected_poll_overflow_remaining = 0;
};

std::error_code open_watch_session(
    const std::filesystem::path& root,
    WatchSession& out_session);
void close_watch_session(WatchSession& session);
std::error_code poll_watch_session_once(
    WatchSession& session,
    DWORD timeout_ms,
    std::vector<CoalescedAction>& out_actions);
void inject_next_poll_error(WatchSession& session, std::errc error);
void inject_next_poll_errors(WatchSession& session, std::errc error, int remaining_failures);
void inject_next_poll_overflow(WatchSession& session);

}  // namespace kernel::watcher
