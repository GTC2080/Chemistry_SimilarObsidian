// Reason: This file keeps the opaque kernel handle layout private to implementation code.

#pragma once

#include "diagnostics/logger.h"
#include "kernel/types.h"
#include "storage/storage.h"
#include "watcher/session.h"

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

struct WatcherSuppressionEntry {
  std::chrono::steady_clock::time_point expires_at;
  std::string content_revision;
};

struct KernelPaths {
  std::filesystem::path root;
  std::filesystem::path state_dir;
  std::filesystem::path storage_db_path;
  std::filesystem::path recovery_journal_path;
};

struct KernelIndexFault {
  std::string reason;
  int code = 0;
  std::uint64_t at_ns = 0;
};

struct KernelFaultRecord {
  std::string reason;
  int code = 0;
  std::uint64_t at_ns = 0;
};

struct KernelRecentEvent {
  std::string kind;
  std::string detail;
  int code = 0;
  std::uint64_t at_ns = 0;
};

struct KernelRebuildSnapshot {
  std::string result;
  std::uint64_t at_ns = 0;
  std::uint64_t duration_ms = 0;
};

struct KernelRecoverySnapshot {
  std::string outcome;
  bool detected_corrupt_tail = false;
  std::uint64_t at_ns = 0;
};

struct KernelContinuityFallbackSnapshot {
  std::string reason;
  std::uint64_t at_ns = 0;
};

struct KernelRuntimeState {
  std::uint64_t pending_recovery_ops = 0;
  std::uint64_t indexed_note_count = 0;
  kernel_session_state session_state = KERNEL_SESSION_OPEN;
  kernel_index_state index_state = KERNEL_INDEX_UNAVAILABLE;
  bool initial_catch_up_complete = false;
  bool rebuild_in_progress = false;
  std::uint64_t next_rebuild_generation = 1;
  std::uint64_t current_rebuild_generation = 0;
  std::uint64_t last_completed_rebuild_generation = 0;
  kernel_error_code last_completed_rebuild_result = KERNEL_ERROR_NOT_FOUND;
  std::uint64_t rebuild_started_at_ns = 0;
  bool background_rebuild_result_ready = false;
  kernel_error_code background_rebuild_result = KERNEL_OK;
  KernelIndexFault index_fault;
  std::vector<KernelFaultRecord> index_fault_history;
  std::vector<KernelRecentEvent> recent_events;
  KernelRebuildSnapshot last_rebuild;
  KernelRecoverySnapshot last_recovery;
  KernelContinuityFallbackSnapshot last_continuity_fallback;
  std::unordered_map<std::string, WatcherSuppressionEntry> suppressed_watcher_paths;
};

struct kernel_handle {
  KernelPaths paths;
  std::mutex runtime_mutex;
  std::condition_variable runtime_cv;
  std::mutex storage_mutex;
  std::unique_ptr<kernel::diagnostics::Logger> logger;
  kernel::storage::Database storage;
  kernel::watcher::WatchSession watcher_session;
  std::jthread watcher_thread;
  std::jthread rebuild_thread;
  KernelRuntimeState runtime;
};
