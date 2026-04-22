// Reason: This file provides the low-level helpers used across the split kernel implementation files.

#include "core/kernel_shared.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <new>

namespace kernel::core {

namespace {

constexpr std::size_t kMaxFaultHistoryEntries = 8;
constexpr std::size_t kMaxRecentEvents = 16;

void append_recent_event_locked(
    KernelRuntimeState& runtime,
    std::string_view kind,
    std::string_view detail,
    const int code,
    const std::uint64_t at_ns) {
  runtime.recent_events.push_back(
      KernelRecentEvent{std::string(kind), std::string(detail), code, at_ns});
  if (runtime.recent_events.size() > kMaxRecentEvents) {
    runtime.recent_events.erase(runtime.recent_events.begin());
  }
}

}  // namespace

kernel_status make_status(const kernel_error_code code) {
  return kernel_status{code};
}

kernel_error_code map_error(const std::error_code& ec) {
  if (!ec) {
    return KERNEL_OK;
  }
  if (ec == std::errc::invalid_argument) {
    return KERNEL_ERROR_INVALID_ARGUMENT;
  }
  if (ec == std::errc::no_such_file_or_directory) {
    return KERNEL_ERROR_NOT_FOUND;
  }
  return KERNEL_ERROR_IO;
}

bool is_null_or_empty(const char* value) {
  return value == nullptr || value[0] == '\0';
}

bool is_valid_relative_path(const char* rel_path) {
  if (is_null_or_empty(rel_path)) {
    return false;
  }

  const std::filesystem::path path(rel_path);
  if (path.is_absolute() || path.has_root_name() || path.has_root_directory()) {
    return false;
  }

  return std::none_of(path.begin(), path.end(), [](const auto& part) {
    return part == "..";
  });
}

std::filesystem::path resolve_note_path(kernel_handle* handle, const char* rel_path) {
  return (handle->paths.root / std::filesystem::path(rel_path)).lexically_normal();
}

std::string normalize_rel_path(std::string_view rel_path) {
  return std::filesystem::path(rel_path).lexically_normal().generic_string();
}

std::uint64_t now_ns() {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::system_clock::now().time_since_epoch())
          .count());
}

void clear_index_fault(kernel_handle* handle) {
  std::lock_guard lock(handle->runtime_mutex);
  handle->runtime.index_fault.reason.clear();
  handle->runtime.index_fault.code = 0;
  handle->runtime.index_fault.at_ns = 0;
}

void set_index_fault(kernel_handle* handle, std::string_view reason, const int code) {
  std::lock_guard lock(handle->runtime_mutex);
  if (handle->runtime.index_fault.reason == reason &&
      handle->runtime.index_fault.code == code &&
      handle->runtime.index_fault.at_ns != 0) {
    return;
  }
  handle->runtime.index_fault.reason = reason;
  handle->runtime.index_fault.code = code;
  handle->runtime.index_fault.at_ns = now_ns();
  handle->runtime.index_fault_history.push_back(
      KernelFaultRecord{std::string(reason), code, handle->runtime.index_fault.at_ns});
  if (handle->runtime.index_fault_history.size() > kMaxFaultHistoryEntries) {
    handle->runtime.index_fault_history.erase(handle->runtime.index_fault_history.begin());
  }
  append_recent_event_locked(
      handle->runtime,
      "live_fault",
      reason,
      code,
      handle->runtime.index_fault.at_ns);
}

void record_recent_event(
    kernel_handle* handle,
    std::string_view kind,
    std::string_view detail,
    const int code) {
  std::lock_guard lock(handle->runtime_mutex);
  append_recent_event_locked(handle->runtime, kind, detail, code, now_ns());
}

void record_rebuild_started(kernel_handle* handle, std::string_view detail) {
  record_recent_event(handle, "rebuild_started", detail, 0);
}

void record_rebuild_result(
    kernel_handle* handle,
    std::string_view result,
    const std::uint64_t started_at_ns,
    const std::uint64_t completed_at_ns,
    const int code) {
  std::lock_guard lock(handle->runtime_mutex);
  handle->runtime.last_rebuild.result = result;
  handle->runtime.last_rebuild.at_ns = completed_at_ns;
  handle->runtime.last_rebuild.duration_ms =
      completed_at_ns >= started_at_ns
          ? (completed_at_ns - started_at_ns) / 1000000
          : 0;
  append_recent_event_locked(
      handle->runtime,
      result == "failed" ? "rebuild_failed" : "rebuild_succeeded",
      result,
      code,
      completed_at_ns);
}

void record_attachment_recount(
    kernel_handle* handle,
    std::string_view reason,
    const std::uint64_t at_ns) {
  std::lock_guard lock(handle->runtime_mutex);
  handle->runtime.last_attachment_recount.reason = std::string(reason);
  handle->runtime.last_attachment_recount.at_ns = at_ns != 0 ? at_ns : now_ns();
}

void record_pdf_recount(
    kernel_handle* handle,
    std::string_view reason,
    const std::uint64_t at_ns) {
  std::lock_guard lock(handle->runtime_mutex);
  handle->runtime.last_pdf_recount.reason = std::string(reason);
  handle->runtime.last_pdf_recount.at_ns = at_ns != 0 ? at_ns : now_ns();
}

void record_domain_recount(
    kernel_handle* handle,
    std::string_view reason,
    const std::uint64_t at_ns) {
  std::lock_guard lock(handle->runtime_mutex);
  handle->runtime.last_domain_recount.reason = std::string(reason);
  handle->runtime.last_domain_recount.at_ns = at_ns != 0 ? at_ns : now_ns();
}

void record_chemistry_recount(
    kernel_handle* handle,
    std::string_view reason,
    const std::uint64_t at_ns) {
  std::lock_guard lock(handle->runtime_mutex);
  handle->runtime.last_chemistry_recount.reason = std::string(reason);
  handle->runtime.last_chemistry_recount.at_ns = at_ns != 0 ? at_ns : now_ns();
}

void record_continuity_fallback(kernel_handle* handle, std::string_view reason) {
  std::lock_guard lock(handle->runtime_mutex);
  handle->runtime.last_continuity_fallback.reason = std::string(reason);
  handle->runtime.last_continuity_fallback.at_ns = now_ns();
  append_recent_event_locked(
      handle->runtime,
      "continuity_fallback",
      reason,
      0,
      handle->runtime.last_continuity_fallback.at_ns);
}

const char* session_state_name(const kernel_session_state state) {
  switch (state) {
    case KERNEL_SESSION_CLOSED:
      return "CLOSED";
    case KERNEL_SESSION_OPEN:
      return "OPEN";
    case KERNEL_SESSION_FAULTED:
      return "FAULTED";
  }
  return "UNKNOWN";
}

const char* index_state_name(const kernel_index_state state) {
  switch (state) {
    case KERNEL_INDEX_UNAVAILABLE:
      return "UNAVAILABLE";
    case KERNEL_INDEX_CATCHING_UP:
      return "CATCHING_UP";
    case KERNEL_INDEX_READY:
      return "READY";
    case KERNEL_INDEX_REBUILDING:
      return "REBUILDING";
  }
  return "UNKNOWN";
}

std::string json_escape(std::string_view value) {
  std::string escaped;
  escaped.reserve(value.size() + 8);
  for (const char ch : value) {
    switch (ch) {
      case '\\':
        escaped += "\\\\";
        break;
      case '"':
        escaped += "\\\"";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        escaped.push_back(ch);
        break;
    }
  }
  return escaped;
}

void copy_revision(const std::string& revision, kernel_note_metadata* out_metadata) {
  std::memset(out_metadata->content_revision, 0, sizeof(out_metadata->content_revision));
  const std::size_t count = std::min(
      revision.size(),
      static_cast<std::size_t>(KERNEL_REVISION_MAX - 1));
  std::memcpy(out_metadata->content_revision, revision.data(), count);
}

void fill_metadata(
    const kernel::platform::FileStat& stat,
    const std::string& revision,
    kernel_note_metadata* out_metadata) {
  out_metadata->file_size = stat.file_size;
  out_metadata->mtime_ns = stat.mtime_ns;
  copy_revision(revision, out_metadata);
}

char* duplicate_c_string(std::string_view value) {
  auto* owned = new (std::nothrow) char[value.size() + 1];
  if (owned == nullptr) {
    return nullptr;
  }

  if (!value.empty()) {
    std::memcpy(owned, value.data(), value.size());
  }
  owned[value.size()] = '\0';
  return owned;
}

void free_search_results_impl(kernel_search_results* results) {
  if (results == nullptr) {
    return;
  }

  if (results->hits != nullptr) {
    for (size_t index = 0; index < results->count; ++index) {
      delete[] results->hits[index].rel_path;
      delete[] results->hits[index].title;
    }
    delete[] results->hits;
  }

  results->hits = nullptr;
  results->count = 0;
}

}  // namespace kernel::core
