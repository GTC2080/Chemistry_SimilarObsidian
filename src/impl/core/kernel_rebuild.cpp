// Reason: This file owns rebuild execution and the host-facing rebuild task APIs.

#include "kernel/c_api.h"

#include "core/kernel_runtime_internal.h"
#include "core/kernel_shared.h"
#include "index/refresh.h"
#include "storage/storage.h"

#include <chrono>

namespace kernel::core {

bool rebuild_in_progress(kernel_handle* handle) {
  std::lock_guard runtime_lock(handle->runtime_mutex);
  return handle->runtime.rebuild_in_progress;
}

void join_completed_background_rebuild_if_needed(kernel_handle* handle) {
  if (!handle->rebuild_thread.joinable()) {
    return;
  }

  bool in_progress = false;
  {
    std::lock_guard runtime_lock(handle->runtime_mutex);
    in_progress = handle->runtime.rebuild_in_progress;
  }
  if (!in_progress) {
    handle->rebuild_thread.join();
  }
}

kernel_error_code run_rebuild(kernel_handle* handle, const std::uint64_t rebuild_started_at_ns) {
  std::lock_guard lock(handle->storage_mutex);
  const std::error_code rebuild_ec =
      kernel::index::full_rescan_markdown_vault(handle->storage, handle->paths.root);
  if (rebuild_ec) {
    const std::uint64_t rebuild_completed_at_ns = now_ns();
    {
      std::lock_guard runtime_lock(handle->runtime_mutex);
      handle->runtime.last_completed_rebuild_generation = handle->runtime.current_rebuild_generation;
      handle->runtime.last_completed_rebuild_result = map_error(rebuild_ec);
      handle->runtime.current_rebuild_generation = 0;
      handle->runtime.rebuild_in_progress = false;
      handle->runtime.rebuild_started_at_ns = 0;
      handle->runtime.index_state = KERNEL_INDEX_UNAVAILABLE;
    }
    set_index_fault(handle, "rebuild_failed", rebuild_ec.value());
    record_rebuild_result(
        handle,
        "failed",
        rebuild_started_at_ns,
        rebuild_completed_at_ns,
        rebuild_ec.value());
    return map_error(rebuild_ec);
  }

  std::uint64_t indexed_note_count = 0;
  const std::error_code count_ec =
      kernel::storage::count_active_notes(handle->storage, indexed_note_count);
  if (count_ec) {
    const std::uint64_t rebuild_completed_at_ns = now_ns();
    {
      std::lock_guard runtime_lock(handle->runtime_mutex);
      handle->runtime.last_completed_rebuild_generation = handle->runtime.current_rebuild_generation;
      handle->runtime.last_completed_rebuild_result = map_error(count_ec);
      handle->runtime.current_rebuild_generation = 0;
      handle->runtime.rebuild_in_progress = false;
      handle->runtime.rebuild_started_at_ns = 0;
      handle->runtime.index_state = KERNEL_INDEX_UNAVAILABLE;
    }
    set_index_fault(handle, "rebuild_failed", count_ec.value());
    record_rebuild_result(
        handle,
        "failed",
        rebuild_started_at_ns,
        rebuild_completed_at_ns,
        count_ec.value());
    return map_error(count_ec);
  }

  const std::uint64_t rebuild_completed_at_ns = now_ns();
  {
    std::lock_guard runtime_lock(handle->runtime_mutex);
    handle->runtime.initial_catch_up_complete = true;
    handle->runtime.last_completed_rebuild_generation = handle->runtime.current_rebuild_generation;
    handle->runtime.last_completed_rebuild_result = KERNEL_OK;
    handle->runtime.current_rebuild_generation = 0;
    handle->runtime.rebuild_in_progress = false;
    handle->runtime.rebuild_started_at_ns = 0;
    handle->runtime.index_state = KERNEL_INDEX_READY;
    handle->runtime.indexed_note_count = indexed_note_count;
  }
  clear_index_fault(handle);
  record_attachment_recount(handle, "rebuild", rebuild_completed_at_ns);
  record_pdf_recount(handle, "rebuild", rebuild_completed_at_ns);
  record_domain_recount(handle, "rebuild", rebuild_completed_at_ns);
  record_chemistry_recount(handle, "rebuild", rebuild_completed_at_ns);
  record_rebuild_result(
      handle,
      "succeeded",
      rebuild_started_at_ns,
      rebuild_completed_at_ns,
      0);
  return KERNEL_OK;
}

}  // namespace kernel::core

extern "C" kernel_status kernel_start_rebuild_index(kernel_handle* handle) {
  if (handle == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel::core::join_completed_background_rebuild_if_needed(handle);

  const std::uint64_t rebuild_started_at_ns = kernel::core::now_ns();
  {
    std::lock_guard runtime_lock(handle->runtime_mutex);
    if (handle->runtime.rebuild_in_progress) {
      return kernel::core::make_status(KERNEL_ERROR_CONFLICT);
    }
    handle->runtime.current_rebuild_generation = handle->runtime.next_rebuild_generation++;
    handle->runtime.rebuild_in_progress = true;
    handle->runtime.rebuild_started_at_ns = rebuild_started_at_ns;
    handle->runtime.index_state = KERNEL_INDEX_REBUILDING;
    handle->runtime.background_rebuild_result_ready = false;
    handle->runtime.background_rebuild_result = KERNEL_OK;
  }
  kernel::core::clear_index_fault(handle);
  kernel::core::record_rebuild_started(handle, "background");

  handle->rebuild_thread = std::jthread([handle, rebuild_started_at_ns]() {
    const kernel_error_code result = kernel::core::run_rebuild(handle, rebuild_started_at_ns);
    {
      std::lock_guard runtime_lock(handle->runtime_mutex);
      handle->runtime.background_rebuild_result = result;
      handle->runtime.background_rebuild_result_ready = true;
    }
    handle->runtime_cv.notify_all();
  });

  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_join_rebuild_index(kernel_handle* handle) {
  if (handle == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  {
    std::lock_guard runtime_lock(handle->runtime_mutex);
    if (!handle->runtime.rebuild_in_progress &&
        !handle->runtime.background_rebuild_result_ready &&
        !handle->rebuild_thread.joinable()) {
      return kernel::core::make_status(KERNEL_ERROR_NOT_FOUND);
    }
  }

  if (handle->rebuild_thread.joinable()) {
    handle->rebuild_thread.join();
  }

  std::lock_guard runtime_lock(handle->runtime_mutex);
  if (!handle->runtime.background_rebuild_result_ready) {
    return kernel::core::make_status(KERNEL_OK);
  }
  return kernel::core::make_status(handle->runtime.background_rebuild_result);
}

extern "C" kernel_status kernel_wait_for_rebuild(kernel_handle* handle, uint32_t timeout_ms) {
  if (handle == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::unique_lock runtime_lock(handle->runtime_mutex);
  if (!handle->runtime.rebuild_in_progress &&
      !handle->runtime.background_rebuild_result_ready &&
      !handle->rebuild_thread.joinable()) {
    return kernel::core::make_status(KERNEL_ERROR_NOT_FOUND);
  }
  if (handle->runtime.rebuild_in_progress) {
    const bool completed = handle->runtime_cv.wait_for(
        runtime_lock,
        std::chrono::milliseconds(timeout_ms),
        [&]() { return !handle->runtime.rebuild_in_progress; });
    if (!completed) {
      return kernel::core::make_status(KERNEL_ERROR_TIMEOUT);
    }
  }

  const bool result_ready = handle->runtime.background_rebuild_result_ready;
  const kernel_error_code result = handle->runtime.background_rebuild_result;
  runtime_lock.unlock();

  if (handle->rebuild_thread.joinable()) {
    handle->rebuild_thread.join();
  }

  if (!result_ready) {
    return kernel::core::make_status(KERNEL_OK);
  }
  return kernel::core::make_status(result);
}

extern "C" kernel_status kernel_get_rebuild_status(
    kernel_handle* handle,
    kernel_rebuild_status_snapshot* out_status) {
  if (handle == nullptr || out_status == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::lock_guard runtime_lock(handle->runtime_mutex);
  out_status->in_flight = handle->runtime.rebuild_in_progress ? 1 : 0;
  out_status->has_last_result =
      handle->runtime.last_completed_rebuild_generation != 0 ? 1 : 0;
  out_status->current_generation = handle->runtime.current_rebuild_generation;
  out_status->last_completed_generation = handle->runtime.last_completed_rebuild_generation;
  out_status->current_started_at_ns =
      handle->runtime.rebuild_in_progress ? handle->runtime.rebuild_started_at_ns : 0;
  out_status->last_result_code = handle->runtime.last_completed_rebuild_result;
  out_status->last_result_at_ns = handle->runtime.last_rebuild.at_ns;
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_rebuild_index(kernel_handle* handle) {
  if (handle == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel::core::join_completed_background_rebuild_if_needed(handle);

  const std::uint64_t rebuild_started_at_ns = kernel::core::now_ns();

  {
    std::lock_guard runtime_lock(handle->runtime_mutex);
    if (handle->runtime.rebuild_in_progress) {
      return kernel::core::make_status(KERNEL_ERROR_CONFLICT);
    }
    handle->runtime.current_rebuild_generation = handle->runtime.next_rebuild_generation++;
    handle->runtime.rebuild_in_progress = true;
    handle->runtime.rebuild_started_at_ns = rebuild_started_at_ns;
    handle->runtime.index_state = KERNEL_INDEX_REBUILDING;
    handle->runtime.background_rebuild_result_ready = false;
    handle->runtime.background_rebuild_result = KERNEL_OK;
  }
  kernel::core::clear_index_fault(handle);
  kernel::core::record_rebuild_started(handle, "sync");
  const kernel_error_code result = kernel::core::run_rebuild(handle, rebuild_started_at_ns);
  {
    std::lock_guard runtime_lock(handle->runtime_mutex);
    handle->runtime.background_rebuild_result = result;
    handle->runtime.background_rebuild_result_ready = true;
  }
  return kernel::core::make_status(result);
}
