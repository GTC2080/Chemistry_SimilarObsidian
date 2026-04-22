// Reason: This file implements the minimal one-shot Windows directory watch session over ReadDirectoryChangesW.

#include "watcher/session.h"

#include "watcher/windows_decode.h"

namespace kernel::watcher {
namespace {

constexpr DWORD kNotifyFilter =
    FILE_NOTIFY_CHANGE_FILE_NAME |
    FILE_NOTIFY_CHANGE_LAST_WRITE;

std::error_code win32_error(const DWORD code) {
  return std::error_code(static_cast<int>(code), std::system_category());
}

std::error_code arm_watch_request(WatchSession& session) {
  ResetEvent(session.event_handle);
  session.overlapped = {};
  session.overlapped.hEvent = session.event_handle;

  if (!ReadDirectoryChangesW(
          session.directory_handle,
          session.buffer.data(),
          static_cast<DWORD>(session.buffer.size()),
          TRUE,
          kNotifyFilter,
          nullptr,
          &session.overlapped,
          nullptr)) {
    return win32_error(GetLastError());
  }

  session.watch_armed = true;
  return {};
}

}  // namespace

std::error_code open_watch_session(
    const std::filesystem::path& root,
    WatchSession& out_session) {
  close_watch_session(out_session);

  const auto handle = CreateFileW(
      root.wstring().c_str(),
      FILE_LIST_DIRECTORY,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
      nullptr,
      OPEN_EXISTING,
      FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
      nullptr);
  if (handle == INVALID_HANDLE_VALUE) {
    return win32_error(GetLastError());
  }

  const auto event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (event == nullptr) {
    const DWORD error = GetLastError();
    CloseHandle(handle);
    return win32_error(error);
  }

  out_session.directory_handle = handle;
  out_session.event_handle = event;
  out_session.root = root;
  out_session.buffer.assign(64 * 1024, std::byte{0});
  out_session.overlapped = {};
  out_session.watch_armed = false;
  return arm_watch_request(out_session);
}

void close_watch_session(WatchSession& session) {
  if (session.directory_handle != INVALID_HANDLE_VALUE && session.watch_armed) {
    CancelIoEx(session.directory_handle, &session.overlapped);
  }
  if (session.directory_handle != INVALID_HANDLE_VALUE) {
    CloseHandle(session.directory_handle);
    session.directory_handle = INVALID_HANDLE_VALUE;
  }
  if (session.event_handle != nullptr) {
    CloseHandle(session.event_handle);
    session.event_handle = nullptr;
  }
  session.root.clear();
  session.buffer.clear();
  session.overlapped = {};
  session.watch_armed = false;
}

std::error_code poll_watch_session_once(
    WatchSession& session,
    const DWORD timeout_ms,
    std::vector<CoalescedAction>& out_actions) {
  out_actions.clear();

  const int remaining_injected_errors =
      session.injected_poll_error_remaining.load(std::memory_order_relaxed);
  if (remaining_injected_errors > 0) {
    session.injected_poll_error_remaining.fetch_sub(1, std::memory_order_relaxed);
    return std::error_code(
        session.injected_poll_error_value.load(std::memory_order_relaxed),
        std::generic_category());
  }
  session.injected_poll_error_value.store(0, std::memory_order_relaxed);

  if (session.directory_handle == INVALID_HANDLE_VALUE || session.event_handle == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }
  if (!session.watch_armed) {
    const std::error_code arm_ec = arm_watch_request(session);
    if (arm_ec) {
      return arm_ec;
    }
  }

  const int remaining_injected_overflows =
      session.injected_poll_overflow_remaining.load(std::memory_order_relaxed);
  if (remaining_injected_overflows > 0) {
    session.injected_poll_overflow_remaining.fetch_sub(1, std::memory_order_relaxed);
    out_actions = coalesce_events({make_overflow_event()});
    return {};
  }

  const DWORD wait_result = WaitForSingleObject(session.event_handle, timeout_ms);
  if (wait_result == WAIT_TIMEOUT) {
    return {};
  }
  if (wait_result != WAIT_OBJECT_0) {
    return win32_error(GetLastError());
  }

  DWORD bytes_returned = 0;
  session.watch_armed = false;
  if (!GetOverlappedResult(session.directory_handle, &session.overlapped, &bytes_returned, FALSE)) {
    const DWORD error = GetLastError();
    if (error == ERROR_NOTIFY_ENUM_DIR) {
      out_actions = coalesce_events({make_overflow_event()});
      return arm_watch_request(session);
    }
    return win32_error(error);
  }

  const auto raw_events = decode_win32_notify_buffer(session.buffer.data(), bytes_returned);
  out_actions = coalesce_events(raw_events);
  return arm_watch_request(session);
}

void inject_next_poll_error(WatchSession& session, const std::errc error) {
  inject_next_poll_errors(session, error, 1);
}

void inject_next_poll_errors(
    WatchSession& session,
    const std::errc error,
    const int remaining_failures) {
  session.injected_poll_error_value.store(
      std::make_error_code(error).value(),
      std::memory_order_relaxed);
  session.injected_poll_error_remaining.store(
      remaining_failures > 0 ? remaining_failures : 0,
      std::memory_order_relaxed);
}

void inject_next_poll_overflow(WatchSession& session) {
  session.injected_poll_overflow_remaining.store(1, std::memory_order_relaxed);
}

}  // namespace kernel::watcher
