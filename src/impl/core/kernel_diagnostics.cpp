// Reason: This file owns diagnostics snapshot export so runtime and rebuild-task reporting does not bloat the main ABI facade.

#include "kernel/c_api.h"

#include "core/kernel_attachment_api_shared.h"
#include "core/kernel_internal.h"
#include "core/kernel_shared.h"
#include "platform/platform.h"
#include "recovery/journal.h"
#include "search/search.h"
#include "storage/storage.h"

#include <filesystem>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr std::size_t kAttachmentAnomalyPathSummaryLimit = 16;

struct AttachmentDiagnosticsSnapshot {
  std::uint64_t attachment_count = 0;
  std::uint64_t missing_attachment_count = 0;
  std::uint64_t orphaned_attachment_count = 0;
  std::vector<std::string> missing_attachment_paths;
  std::vector<std::string> orphaned_attachment_paths;
};

std::string build_fault_history_json(
    const std::vector<KernelFaultRecord>& fault_history) {
  std::ostringstream output;
  output << "[";
  for (std::size_t index = 0; index < fault_history.size(); ++index) {
    if (index != 0) {
      output << ",";
    }
    output << "{"
           << "\"reason\":\"" << kernel::core::json_escape(fault_history[index].reason) << "\","
           << "\"code\":" << fault_history[index].code << ","
           << "\"at_ns\":" << fault_history[index].at_ns
           << "}";
  }
  output << "]";
  return output.str();
}

std::string build_string_array_json(const std::vector<std::string>& values) {
  std::ostringstream output;
  output << "[";
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index != 0) {
      output << ",";
    }
    output << "\"" << kernel::core::json_escape(values[index]) << "\"";
  }
  output << "]";
  return output.str();
}

std::string build_recent_events_json(
    const std::vector<KernelRecentEvent>& recent_events) {
  std::ostringstream output;
  output << "[";
  for (std::size_t index = 0; index < recent_events.size(); ++index) {
    if (index != 0) {
      output << ",";
    }
    output << "{"
           << "\"kind\":\"" << kernel::core::json_escape(recent_events[index].kind) << "\","
           << "\"detail\":\"" << kernel::core::json_escape(recent_events[index].detail) << "\","
           << "\"code\":" << recent_events[index].code << ","
           << "\"at_ns\":" << recent_events[index].at_ns
           << "}";
  }
  output << "]";
  return output.str();
}

std::string_view attachment_anomaly_summary(
    const AttachmentDiagnosticsSnapshot& snapshot) {
  const bool has_missing = snapshot.missing_attachment_count != 0;
  const bool has_orphaned = snapshot.orphaned_attachment_count != 0;
  if (has_missing && has_orphaned) {
    return "missing_live_and_orphaned_attachments";
  }
  if (has_missing) {
    return "missing_live_attachments";
  }
  if (has_orphaned) {
    return "orphaned_attachments";
  }
  return "clean";
}

kernel_status try_collect_attachment_diagnostics_snapshot(
    kernel_handle* handle,
    const kernel_index_state index_state,
    AttachmentDiagnosticsSnapshot& out_snapshot) {
  out_snapshot = AttachmentDiagnosticsSnapshot{};

  const bool attachment_counts_stable =
      index_state != KERNEL_INDEX_CATCHING_UP &&
      index_state != KERNEL_INDEX_REBUILDING;
  if (!attachment_counts_stable) {
    return kernel::core::make_status(KERNEL_OK);
  }

  std::unique_lock<std::mutex> storage_lock(handle->storage_mutex, std::defer_lock);
  const auto attachment_count_deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(20);
  while (!storage_lock.try_lock()) {
    if (std::chrono::steady_clock::now() >= attachment_count_deadline) {
      return kernel::core::make_status(KERNEL_OK);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }

  const std::error_code attachment_count_ec =
      kernel::storage::count_attachments(handle->storage, out_snapshot.attachment_count);
  if (attachment_count_ec) {
    return kernel::core::make_status(kernel::core::map_error(attachment_count_ec));
  }

  const std::error_code missing_attachment_count_ec =
      kernel::storage::count_missing_attachments(
          handle->storage,
          out_snapshot.missing_attachment_count);
  if (missing_attachment_count_ec) {
    return kernel::core::make_status(kernel::core::map_error(missing_attachment_count_ec));
  }

  const std::error_code orphaned_attachment_count_ec =
      kernel::storage::count_orphaned_attachments(
          handle->storage,
          out_snapshot.orphaned_attachment_count);
  if (orphaned_attachment_count_ec) {
    return kernel::core::make_status(kernel::core::map_error(orphaned_attachment_count_ec));
  }

  const std::error_code missing_attachment_paths_ec =
      kernel::storage::list_missing_attachment_paths(
          handle->storage,
          kAttachmentAnomalyPathSummaryLimit,
          out_snapshot.missing_attachment_paths);
  if (missing_attachment_paths_ec) {
    return kernel::core::make_status(kernel::core::map_error(missing_attachment_paths_ec));
  }

  const std::error_code orphaned_attachment_paths_ec =
      kernel::storage::list_orphaned_attachment_paths(
          handle->storage,
          kAttachmentAnomalyPathSummaryLimit,
          out_snapshot.orphaned_attachment_paths);
  if (orphaned_attachment_paths_ec) {
    return kernel::core::make_status(kernel::core::map_error(orphaned_attachment_paths_ec));
  }

  return kernel::core::make_status(KERNEL_OK);
}

std::string build_diagnostics_json(
    const kernel_session_state session_state,
    const kernel_index_state index_state,
    const bool rebuild_in_flight,
    const bool has_last_rebuild_result,
    const std::uint64_t rebuild_current_generation,
    const std::uint64_t rebuild_last_completed_generation,
    const std::uint64_t rebuild_current_started_at_ns,
    std::string_view last_recovery_outcome,
    const bool last_recovery_detected_corrupt_tail,
    const std::uint64_t last_recovery_at_ns,
    const kernel_error_code last_rebuild_result_code,
    std::string_view index_fault_reason,
    const int index_fault_code,
    const std::uint64_t index_fault_at_ns,
    const std::vector<KernelFaultRecord>& fault_history,
    const std::vector<KernelRecentEvent>& recent_events,
    std::string_view last_rebuild_result,
    const std::uint64_t last_rebuild_at_ns,
    const std::uint64_t last_rebuild_duration_ms,
    const std::uint64_t indexed_note_count,
    const AttachmentDiagnosticsSnapshot& attachment_snapshot,
    std::string_view last_attachment_recount_reason,
    const std::uint64_t last_attachment_recount_at_ns,
    std::string_view last_continuity_fallback_reason,
    const std::uint64_t last_continuity_fallback_at_ns,
    const std::uint64_t pending_recovery_ops,
    std::string_view logger_backend,
    const std::filesystem::path& root,
    const std::filesystem::path& state_dir,
    const std::filesystem::path& storage_db_path,
    const std::filesystem::path& recovery_journal_path) {
  std::ostringstream output;
  const auto generated_at_ns = kernel::core::now_ns();
  output << "{\n"
         << "  \"generated_at_ns\":" << generated_at_ns << ",\n"
         << "  \"session_state\":\"" << kernel::core::session_state_name(session_state) << "\",\n"
         << "  \"index_state\":\"" << kernel::core::index_state_name(index_state) << "\",\n"
         << "  \"rebuild_in_flight\":" << (rebuild_in_flight ? "true" : "false") << ",\n"
         << "  \"has_last_rebuild_result\":" << (has_last_rebuild_result ? "true" : "false") << ",\n"
         << "  \"rebuild_current_generation\":" << rebuild_current_generation << ",\n"
         << "  \"rebuild_last_completed_generation\":" << rebuild_last_completed_generation << ",\n"
         << "  \"rebuild_current_started_at_ns\":" << rebuild_current_started_at_ns << ",\n"
         << "  \"last_recovery_outcome\":\"" << kernel::core::json_escape(last_recovery_outcome) << "\",\n"
         << "  \"last_recovery_detected_corrupt_tail\":"
         << (last_recovery_detected_corrupt_tail ? "true" : "false") << ",\n"
         << "  \"last_recovery_at_ns\":" << last_recovery_at_ns << ",\n"
         << "  \"last_rebuild_result_code\":" << static_cast<int>(last_rebuild_result_code) << ",\n"
         << "  \"index_fault_reason\":\"" << kernel::core::json_escape(index_fault_reason) << "\",\n"
         << "  \"index_fault_code\":" << index_fault_code << ",\n"
         << "  \"index_fault_at_ns\":" << index_fault_at_ns << ",\n"
         << "  \"index_fault_history\":" << build_fault_history_json(fault_history) << ",\n"
         << "  \"recent_events\":" << build_recent_events_json(recent_events) << ",\n"
         << "  \"last_rebuild_result\":\"" << kernel::core::json_escape(last_rebuild_result) << "\",\n"
         << "  \"last_rebuild_at_ns\":" << last_rebuild_at_ns << ",\n"
         << "  \"last_rebuild_duration_ms\":" << last_rebuild_duration_ms << ",\n"
         << "  \"indexed_note_count\":" << indexed_note_count << ",\n"
         << "  \"attachment_count\":" << attachment_snapshot.attachment_count << ",\n"
         << "  \"attachment_live_count\":" << attachment_snapshot.attachment_count << ",\n"
         << "  \"missing_attachment_count\":" << attachment_snapshot.missing_attachment_count << ",\n"
         << "  \"orphaned_attachment_count\":" << attachment_snapshot.orphaned_attachment_count << ",\n"
         << "  \"missing_attachment_paths\":"
         << build_string_array_json(attachment_snapshot.missing_attachment_paths) << ",\n"
         << "  \"orphaned_attachment_paths\":"
         << build_string_array_json(attachment_snapshot.orphaned_attachment_paths) << ",\n"
         << "  \"attachment_anomaly_path_summary_limit\":"
         << kAttachmentAnomalyPathSummaryLimit << ",\n"
         << "  \"attachment_anomaly_summary\":\""
         << kernel::core::json_escape(attachment_anomaly_summary(attachment_snapshot))
         << "\",\n"
         << "  \"last_attachment_recount_reason\":\""
         << kernel::core::json_escape(last_attachment_recount_reason) << "\",\n"
         << "  \"last_attachment_recount_at_ns\":" << last_attachment_recount_at_ns << ",\n"
         << "  \"attachment_public_surface_revision\":\""
         << kernel::core::json_escape(kernel::core::attachment_api::kAttachmentPublicSurfaceRevision)
         << "\",\n"
         << "  \"attachment_metadata_contract_revision\":\""
         << kernel::core::json_escape(
                kernel::core::attachment_api::kAttachmentMetadataContractRevision)
         << "\",\n"
         << "  \"attachment_kind_mapping_revision\":\""
         << kernel::core::json_escape(kernel::core::attachment_api::kAttachmentKindMappingRevision)
         << "\",\n"
         << "  \"last_continuity_fallback_reason\":\""
         << kernel::core::json_escape(last_continuity_fallback_reason) << "\",\n"
         << "  \"last_continuity_fallback_at_ns\":" << last_continuity_fallback_at_ns << ",\n"
         << "  \"pending_recovery_ops\":" << pending_recovery_ops << ",\n"
         << "  \"logger_backend\":\"" << kernel::core::json_escape(logger_backend) << "\",\n"
         << "  \"search_contract_revision\":\""
         << kernel::core::json_escape(kernel::search::kSearchContractRevision) << "\",\n"
         << "  \"search_backend\":\""
         << kernel::core::json_escape(kernel::search::kSearchBackend) << "\",\n"
         << "  \"search_snippet_mode\":\""
         << kernel::core::json_escape(kernel::search::kSearchSnippetMode) << "\",\n"
         << "  \"search_snippet_max_bytes\":" << kernel::search::kSearchSnippetMaxBytes << ",\n"
         << "  \"search_pagination_mode\":\""
         << kernel::core::json_escape(kernel::search::kSearchPaginationMode) << "\",\n"
         << "  \"search_filters_mode\":\""
         << kernel::core::json_escape(kernel::search::kSearchFiltersMode) << "\",\n"
         << "  \"search_ranking_mode\":\""
         << kernel::core::json_escape(kernel::search::kSearchRankingMode) << "\",\n"
         << "  \"search_supported_kinds\":\""
         << kernel::core::json_escape(kernel::search::kSearchSupportedKinds) << "\",\n"
         << "  \"search_supported_filters\":\""
         << kernel::core::json_escape(kernel::search::kSearchSupportedFilters) << "\",\n"
         << "  \"search_ranking_supported_kinds\":\""
         << kernel::core::json_escape(kernel::search::kSearchRankingSupportedKinds) << "\",\n"
         << "  \"search_ranking_tie_break\":\""
         << kernel::core::json_escape(kernel::search::kSearchRankingTieBreak) << "\",\n"
         << "  \"search_page_max_limit\":" << kernel::search::kSearchPageMaxLimit << ",\n"
         << "  \"search_total_hits_supported\":"
         << (kernel::search::kSearchTotalHitsSupported ? "true" : "false") << ",\n"
         << "  \"search_include_deleted_supported\":"
         << (kernel::search::kSearchIncludeDeletedSupported ? "true" : "false") << ",\n"
         << "  \"search_attachment_path_only\":"
         << (kernel::search::kSearchAttachmentPathOnly ? "true" : "false") << ",\n"
         << "  \"search_title_hit_boost_enabled\":"
         << (kernel::search::kSearchTitleHitBoostEnabled ? "true" : "false") << ",\n"
         << "  \"search_tag_exact_boost_enabled\":"
         << (kernel::search::kSearchTagExactBoostEnabled ? "true" : "false") << ",\n"
         << "  \"search_tag_exact_boost_single_token_only\":"
         << (kernel::search::kSearchTagExactBoostSingleTokenOnly ? "true" : "false") << ",\n"
         << "  \"search_all_kind_order\":\""
         << kernel::core::json_escape(kernel::search::kSearchAllKindOrder) << "\",\n"
         << "  \"vault_root\":\"" << kernel::core::json_escape(root.generic_string()) << "\",\n"
         << "  \"state_dir\":\"" << kernel::core::json_escape(state_dir.generic_string()) << "\",\n"
         << "  \"storage_db_path\":\"" << kernel::core::json_escape(storage_db_path.generic_string()) << "\",\n"
         << "  \"recovery_journal_path\":\"" << kernel::core::json_escape(recovery_journal_path.generic_string()) << "\"\n"
         << "}\n";
  return output.str();
}

}  // namespace

extern "C" kernel_status kernel_export_diagnostics(kernel_handle* handle, const char* output_path) {
  if (handle == nullptr || kernel::core::is_null_or_empty(output_path)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::uint64_t indexed_note_count = 0;
  std::string index_fault_reason;
  int index_fault_code = 0;
  std::uint64_t index_fault_at_ns = 0;
  bool rebuild_in_flight = false;
  bool has_last_rebuild_result = false;
  std::uint64_t rebuild_current_generation = 0;
  std::uint64_t rebuild_last_completed_generation = 0;
  std::uint64_t rebuild_current_started_at_ns = 0;
  std::string last_recovery_outcome;
  bool last_recovery_detected_corrupt_tail = false;
  std::uint64_t last_recovery_at_ns = 0;
  kernel_error_code last_rebuild_result_code = KERNEL_ERROR_NOT_FOUND;
  std::vector<KernelFaultRecord> fault_history;
  std::vector<KernelRecentEvent> recent_events;
  std::string last_rebuild_result;
  std::uint64_t last_rebuild_at_ns = 0;
  std::uint64_t last_rebuild_duration_ms = 0;
  std::string last_attachment_recount_reason;
  std::uint64_t last_attachment_recount_at_ns = 0;
  std::string last_continuity_fallback_reason;
  std::uint64_t last_continuity_fallback_at_ns = 0;
  std::string logger_backend = "null_logger";
  kernel_session_state session_state = KERNEL_SESSION_OPEN;
  kernel_index_state index_state = KERNEL_INDEX_UNAVAILABLE;
  {
    std::lock_guard runtime_lock(handle->runtime_mutex);
    session_state = handle->runtime.session_state;
    index_state = handle->runtime.index_state;
    index_fault_reason = handle->runtime.index_fault.reason;
    index_fault_code = handle->runtime.index_fault.code;
    index_fault_at_ns = handle->runtime.index_fault.at_ns;
    rebuild_in_flight = handle->runtime.rebuild_in_progress;
    has_last_rebuild_result = handle->runtime.last_completed_rebuild_generation != 0;
    rebuild_current_generation = handle->runtime.current_rebuild_generation;
    rebuild_last_completed_generation = handle->runtime.last_completed_rebuild_generation;
    rebuild_current_started_at_ns =
        handle->runtime.rebuild_in_progress ? handle->runtime.rebuild_started_at_ns : 0;
    last_recovery_outcome = handle->runtime.last_recovery.outcome;
    last_recovery_detected_corrupt_tail =
        handle->runtime.last_recovery.detected_corrupt_tail;
    last_recovery_at_ns = handle->runtime.last_recovery.at_ns;
    last_rebuild_result_code = handle->runtime.last_completed_rebuild_result;
    fault_history = handle->runtime.index_fault_history;
    recent_events = handle->runtime.recent_events;
    last_rebuild_result = handle->runtime.last_rebuild.result;
    last_rebuild_at_ns = handle->runtime.last_rebuild.at_ns;
    last_rebuild_duration_ms = handle->runtime.last_rebuild.duration_ms;
    last_attachment_recount_reason = handle->runtime.last_attachment_recount.reason;
    last_attachment_recount_at_ns = handle->runtime.last_attachment_recount.at_ns;
    last_continuity_fallback_reason = handle->runtime.last_continuity_fallback.reason;
    last_continuity_fallback_at_ns = handle->runtime.last_continuity_fallback.at_ns;
    indexed_note_count = handle->runtime.indexed_note_count;
  }
  if (handle->logger != nullptr) {
    logger_backend = std::string(handle->logger->backend_name());
  }

  std::uint64_t pending_recovery_ops = 0;
  AttachmentDiagnosticsSnapshot attachment_snapshot{};
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

  const kernel_status attachment_snapshot_status =
      try_collect_attachment_diagnostics_snapshot(
          handle,
          index_state,
          attachment_snapshot);
  if (attachment_snapshot_status.code != KERNEL_OK) {
    return attachment_snapshot_status;
  }

  const std::string diagnostics_json = build_diagnostics_json(
      session_state,
      index_state,
      rebuild_in_flight,
      has_last_rebuild_result,
      rebuild_current_generation,
      rebuild_last_completed_generation,
      rebuild_current_started_at_ns,
      last_recovery_outcome,
      last_recovery_detected_corrupt_tail,
      last_recovery_at_ns,
      last_rebuild_result_code,
      index_fault_reason,
      index_fault_code,
      index_fault_at_ns,
      fault_history,
      recent_events,
      last_rebuild_result,
      last_rebuild_at_ns,
      last_rebuild_duration_ms,
      indexed_note_count,
      attachment_snapshot,
      last_attachment_recount_reason,
      last_attachment_recount_at_ns,
      last_continuity_fallback_reason,
      last_continuity_fallback_at_ns,
      pending_recovery_ops,
      logger_backend,
      handle->paths.root,
      handle->paths.state_dir,
      handle->paths.storage_db_path,
      handle->paths.recovery_journal_path);

  const std::filesystem::path target_path = std::filesystem::path(output_path).lexically_normal();
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
