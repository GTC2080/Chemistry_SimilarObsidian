// Reason: This file owns kernel session open/close and the public runtime-state snapshot APIs.

#include "kernel/c_api.h"

#include "core/kernel_internal.h"
#include "core/kernel_runtime_internal.h"
#include "core/kernel_shared.h"
#include "platform/platform.h"
#include "recovery/journal.h"
#include "storage/storage.h"
#include "vault/state_paths.h"
#include "watcher/session.h"

#include <filesystem>
#include <memory>

extern "C" kernel_status kernel_open_vault(const char* vault_path, kernel_handle** out_handle) {
  if (kernel::core::is_null_or_empty(vault_path) || out_handle == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::filesystem::path root = std::filesystem::path(vault_path).lexically_normal();
  bool exists = false;
  const std::error_code exists_ec = kernel::platform::directory_exists(root, exists);
  if (exists_ec) {
    return kernel::core::make_status(kernel::core::map_error(exists_ec));
  }
  if (!exists) {
    return kernel::core::make_status(KERNEL_ERROR_NOT_FOUND);
  }

  auto handle = std::make_unique<kernel_handle>();
  handle->paths.root = root;
  handle->paths.state_dir = kernel::vault::state_dir_for_vault(root);
  handle->paths.storage_db_path = kernel::vault::storage_db_path(handle->paths.state_dir);
  handle->paths.recovery_journal_path = kernel::vault::recovery_journal_path(handle->paths.state_dir);
  {
    std::lock_guard runtime_lock(handle->runtime_mutex);
    handle->runtime.session_state = KERNEL_SESSION_OPEN;
    handle->runtime.index_state = KERNEL_INDEX_CATCHING_UP;
  }
  kernel::core::clear_index_fault(handle.get());
  handle->logger = kernel::diagnostics::make_null_logger();

  const std::error_code state_dir_ec = kernel::platform::ensure_directory(handle->paths.state_dir);
  if (state_dir_ec) {
    return kernel::core::make_status(kernel::core::map_error(state_dir_ec));
  }

  const std::error_code storage_open_ec =
      kernel::storage::open_or_create(handle->paths.storage_db_path, handle->storage);
  if (storage_open_ec) {
    return kernel::core::make_status(kernel::core::map_error(storage_open_ec));
  }

  const std::error_code schema_ec = kernel::storage::ensure_schema_v1(handle->storage);
  if (schema_ec) {
    kernel::storage::close(handle->storage);
    return kernel::core::make_status(kernel::core::map_error(schema_ec));
  }

  kernel::recovery::StartupRecoverySummary recovery_summary;
  const std::error_code recovery_scan_ec =
      kernel::recovery::recover_startup(
          handle->paths.recovery_journal_path,
          handle->paths.root,
          handle->storage,
          handle->runtime.pending_recovery_ops,
          &recovery_summary);
  if (recovery_scan_ec) {
    kernel::storage::close(handle->storage);
    return kernel::core::make_status(kernel::core::map_error(recovery_scan_ec));
  }
  handle->runtime.last_recovery.outcome = std::move(recovery_summary.outcome);
  handle->runtime.last_recovery.detected_corrupt_tail = recovery_summary.detected_corrupt_tail;
  handle->runtime.last_recovery.at_ns = kernel::core::now_ns();
  kernel::core::record_recent_event(
      handle.get(),
      "startup_recovery",
      handle->runtime.last_recovery.outcome,
      0);

  const std::error_code watch_ec =
      kernel::watcher::open_watch_session(handle->paths.root, handle->watcher_session);
  if (watch_ec) {
    kernel::storage::close(handle->storage);
    return kernel::core::make_status(kernel::core::map_error(watch_ec));
  }

  std::uint64_t indexed_note_count = 0;
  const std::error_code count_ec = kernel::storage::count_active_notes(handle->storage, indexed_note_count);
  if (count_ec) {
    kernel::watcher::close_watch_session(handle->watcher_session);
    kernel::storage::close(handle->storage);
    return kernel::core::make_status(kernel::core::map_error(count_ec));
  }

  handle->watcher_thread = std::jthread(kernel::core::watcher_loop, handle.get());

  *out_handle = handle.release();
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_close(kernel_handle* handle) {
  if (handle != nullptr) {
    if (handle->watcher_thread.joinable()) {
      handle->watcher_thread.request_stop();
      handle->watcher_thread.join();
    }
    if (handle->rebuild_thread.joinable()) {
      handle->rebuild_thread.join();
    }
    kernel::watcher::close_watch_session(handle->watcher_session);
    std::lock_guard lock(handle->storage_mutex);
    kernel::storage::close(handle->storage);
  }
  delete handle;
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_get_state(kernel_handle* handle, kernel_state_snapshot* out_state) {
  if (handle == nullptr || out_state == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::uint64_t indexed_note_count = 0;
  kernel_session_state session_state = KERNEL_SESSION_OPEN;
  kernel_index_state index_state = KERNEL_INDEX_UNAVAILABLE;
  {
    std::lock_guard runtime_lock(handle->runtime_mutex);
    session_state = handle->runtime.session_state;
    index_state = handle->runtime.index_state;
    indexed_note_count = handle->runtime.indexed_note_count;
  }

  std::uint64_t pending_recovery_ops = 0;
  const std::error_code pending_ec =
      kernel::recovery::count_unfinished_save_operations(
          handle->paths.recovery_journal_path,
          pending_recovery_ops);
  if (pending_ec) {
    return kernel::core::make_status(kernel::core::map_error(pending_ec));
  }
  {
    std::lock_guard runtime_lock(handle->runtime_mutex);
    handle->runtime.pending_recovery_ops = pending_recovery_ops;
  }

  out_state->session_state = session_state;
  out_state->index_state = index_state;
  out_state->indexed_note_count = indexed_note_count;
  out_state->pending_recovery_ops = pending_recovery_ops;
  return kernel::core::make_status(KERNEL_OK);
}
