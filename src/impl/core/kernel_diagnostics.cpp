// Reason: This file keeps diagnostics export as a thin orchestration layer so
// storage sampling and JSON assembly can live in smaller focused units.

#include "kernel/c_api.h"

#include "core/kernel_diagnostics_support.h"
#include "core/kernel_internal.h"
#include "core/kernel_shared.h"
#include "platform/platform.h"
#include "recovery/journal.h"

#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>

namespace {

kernel::core::diagnostics_export::RuntimeDiagnosticsSnapshot capture_runtime_snapshot(
    kernel_handle* handle) {
  kernel::core::diagnostics_export::RuntimeDiagnosticsSnapshot snapshot{};
  {
    std::lock_guard runtime_lock(handle->runtime_mutex);
    snapshot.session_state = handle->runtime.session_state;
    snapshot.index_state = handle->runtime.index_state;
    snapshot.index_fault_reason = handle->runtime.index_fault.reason;
    snapshot.index_fault_code = handle->runtime.index_fault.code;
    snapshot.index_fault_at_ns = handle->runtime.index_fault.at_ns;
    snapshot.rebuild_in_flight = handle->runtime.rebuild_in_progress;
    snapshot.has_last_rebuild_result =
        handle->runtime.last_completed_rebuild_generation != 0;
    snapshot.rebuild_current_generation = handle->runtime.current_rebuild_generation;
    snapshot.rebuild_last_completed_generation =
        handle->runtime.last_completed_rebuild_generation;
    snapshot.rebuild_current_started_at_ns =
        handle->runtime.rebuild_in_progress ? handle->runtime.rebuild_started_at_ns : 0;
    snapshot.last_recovery_outcome = handle->runtime.last_recovery.outcome;
    snapshot.last_recovery_detected_corrupt_tail =
        handle->runtime.last_recovery.detected_corrupt_tail;
    snapshot.last_recovery_at_ns = handle->runtime.last_recovery.at_ns;
    snapshot.last_rebuild_result_code = handle->runtime.last_completed_rebuild_result;
    snapshot.fault_history = handle->runtime.index_fault_history;
    snapshot.recent_events = handle->runtime.recent_events;
    snapshot.last_rebuild_result = handle->runtime.last_rebuild.result;
    snapshot.last_rebuild_at_ns = handle->runtime.last_rebuild.at_ns;
    snapshot.last_rebuild_duration_ms = handle->runtime.last_rebuild.duration_ms;
    snapshot.last_attachment_recount_reason = handle->runtime.last_attachment_recount.reason;
    snapshot.last_attachment_recount_at_ns = handle->runtime.last_attachment_recount.at_ns;
    snapshot.last_continuity_fallback_reason =
        handle->runtime.last_continuity_fallback.reason;
    snapshot.last_continuity_fallback_at_ns =
        handle->runtime.last_continuity_fallback.at_ns;
    snapshot.indexed_note_count = handle->runtime.indexed_note_count;
  }

  if (handle->logger != nullptr) {
    snapshot.logger_backend = std::string(handle->logger->backend_name());
  }

  snapshot.root = handle->paths.root;
  snapshot.state_dir = handle->paths.state_dir;
  snapshot.storage_db_path = handle->paths.storage_db_path;
  snapshot.recovery_journal_path = handle->paths.recovery_journal_path;
  return snapshot;
}

kernel_status refresh_pending_recovery_ops(
    kernel_handle* handle,
    kernel::core::diagnostics_export::RuntimeDiagnosticsSnapshot& snapshot) {
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
  snapshot.pending_recovery_ops = pending_recovery_ops;
  return kernel::core::make_status(KERNEL_OK);
}

kernel_status write_diagnostics_json_atomically(
    const std::filesystem::path& target_path,
    std::string_view diagnostics_json) {
  std::filesystem::path temp_path;
  const std::error_code temp_ec =
      kernel::platform::write_temp_file(target_path, diagnostics_json, temp_path);
  if (temp_ec) {
    return kernel::core::make_status(kernel::core::map_error(temp_ec));
  }

  const std::error_code replace_ec =
      kernel::platform::atomic_replace_file(temp_path, target_path);
  if (replace_ec) {
    kernel::platform::remove_file_if_exists(temp_path);
    return kernel::core::make_status(kernel::core::map_error(replace_ec));
  }

  return kernel::core::make_status(KERNEL_OK);
}

}  // namespace

extern "C" kernel_status kernel_export_diagnostics(
    kernel_handle* handle,
    const char* output_path) {
  if (handle == nullptr || kernel::core::is_null_or_empty(output_path)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel::core::diagnostics_export::RuntimeDiagnosticsSnapshot runtime_snapshot =
      capture_runtime_snapshot(handle);
  const kernel_status pending_status =
      refresh_pending_recovery_ops(handle, runtime_snapshot);
  if (pending_status.code != KERNEL_OK) {
    return pending_status;
  }

  kernel::core::diagnostics_export::AttachmentDiagnosticsSnapshot attachment_snapshot{};
  const kernel_status attachment_status =
      kernel::core::diagnostics_export::collect_attachment_diagnostics_snapshot(
          handle,
          runtime_snapshot.index_state,
          attachment_snapshot);
  if (attachment_status.code != KERNEL_OK) {
    return attachment_status;
  }

  kernel::core::diagnostics_export::PdfDiagnosticsSnapshot pdf_snapshot{};
  const kernel_status pdf_status =
      kernel::core::diagnostics_export::collect_pdf_diagnostics_snapshot(
          handle,
          runtime_snapshot.index_state,
          pdf_snapshot);
  if (pdf_status.code != KERNEL_OK) {
    return pdf_status;
  }

  const std::string diagnostics_json =
      kernel::core::diagnostics_export::build_diagnostics_json(
          runtime_snapshot,
          attachment_snapshot,
          pdf_snapshot);
  const std::filesystem::path target_path =
      std::filesystem::path(output_path).lexically_normal();
  return write_diagnostics_json_atomically(target_path, diagnostics_json);
}
