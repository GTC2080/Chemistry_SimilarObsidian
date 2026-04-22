// Reason: This file owns diagnostics JSON assembly so the export entrypoint
// does not also carry the large support-bundle string builder.

#include "core/kernel_diagnostics_support.h"

#include "core/kernel_attachment_api_shared.h"
#include "core/kernel_shared.h"
#include "pdf/pdf_anchor.h"
#include "pdf/pdf_metadata.h"
#include "search/search.h"

#include <sstream>
#include <string_view>

namespace kernel::core::diagnostics_export {
namespace {

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

std::string build_pdf_metadata_anomaly_summary(
    const PdfDiagnosticsSnapshot& snapshot) {
  std::string summary;
  const auto append_token = [&](std::string_view token) {
    if (!summary.empty()) {
      summary += "_";
    }
    summary += token;
  };

  if (snapshot.partial_pdf_count != 0) {
    append_token("partial");
  }
  if (snapshot.invalid_pdf_count != 0) {
    append_token("invalid");
  }
  if (snapshot.unavailable_pdf_count != 0) {
    append_token("unavailable");
  }
  if (summary.empty()) {
    return "clean";
  }
  summary += "_pdf_metadata";
  return summary;
}

std::string build_pdf_reference_anomaly_summary(
    const PdfDiagnosticsSnapshot& snapshot) {
  std::string summary;
  const auto append_token = [&](std::string_view token) {
    if (!summary.empty()) {
      summary += "_";
    }
    summary += token;
  };

  if (snapshot.missing_pdf_source_ref_count != 0) {
    append_token("missing");
  }
  if (snapshot.stale_pdf_source_ref_count != 0) {
    append_token("stale");
  }
  if (snapshot.unresolved_pdf_source_ref_count != 0) {
    append_token("unresolved");
  }
  if (summary.empty()) {
    return "clean";
  }
  summary += "_pdf_references";
  return summary;
}

std::string build_domain_unresolved_summary(
    const DomainDiagnosticsSnapshot& snapshot) {
  return snapshot.unresolved_domain_source_ref_count != 0
             ? "domain_source_refs"
             : "clean";
}

std::string build_domain_stale_summary(
    const DomainDiagnosticsSnapshot& snapshot) {
  return snapshot.stale_domain_source_ref_count != 0
             ? "domain_source_refs"
             : "clean";
}

std::string build_domain_unsupported_summary(
    const DomainDiagnosticsSnapshot& snapshot) {
  return snapshot.unsupported_domain_source_ref_count != 0
             ? "domain_source_refs"
             : "clean";
}

}  // namespace

std::string build_diagnostics_json(
    const RuntimeDiagnosticsSnapshot& runtime_snapshot,
    const AttachmentDiagnosticsSnapshot& attachment_snapshot,
    const PdfDiagnosticsSnapshot& pdf_snapshot,
    const DomainDiagnosticsSnapshot& domain_snapshot) {
  std::ostringstream output;
  const auto generated_at_ns = kernel::core::now_ns();
  output << "{\n"
         << "  \"generated_at_ns\":" << generated_at_ns << ",\n"
         << "  \"session_state\":\""
         << kernel::core::session_state_name(runtime_snapshot.session_state) << "\",\n"
         << "  \"index_state\":\""
         << kernel::core::index_state_name(runtime_snapshot.index_state) << "\",\n"
         << "  \"rebuild_in_flight\":"
         << (runtime_snapshot.rebuild_in_flight ? "true" : "false") << ",\n"
         << "  \"has_last_rebuild_result\":"
         << (runtime_snapshot.has_last_rebuild_result ? "true" : "false") << ",\n"
         << "  \"rebuild_current_generation\":"
         << runtime_snapshot.rebuild_current_generation << ",\n"
         << "  \"rebuild_last_completed_generation\":"
         << runtime_snapshot.rebuild_last_completed_generation << ",\n"
         << "  \"rebuild_current_started_at_ns\":"
         << runtime_snapshot.rebuild_current_started_at_ns << ",\n"
         << "  \"last_recovery_outcome\":\""
         << kernel::core::json_escape(runtime_snapshot.last_recovery_outcome) << "\",\n"
         << "  \"last_recovery_detected_corrupt_tail\":"
         << (runtime_snapshot.last_recovery_detected_corrupt_tail ? "true" : "false")
         << ",\n"
         << "  \"last_recovery_at_ns\":" << runtime_snapshot.last_recovery_at_ns << ",\n"
         << "  \"last_rebuild_result_code\":"
         << static_cast<int>(runtime_snapshot.last_rebuild_result_code) << ",\n"
         << "  \"index_fault_reason\":\""
         << kernel::core::json_escape(runtime_snapshot.index_fault_reason) << "\",\n"
         << "  \"index_fault_code\":" << runtime_snapshot.index_fault_code << ",\n"
         << "  \"index_fault_at_ns\":" << runtime_snapshot.index_fault_at_ns << ",\n"
         << "  \"index_fault_history\":"
         << build_fault_history_json(runtime_snapshot.fault_history) << ",\n"
         << "  \"recent_events\":"
         << build_recent_events_json(runtime_snapshot.recent_events) << ",\n"
         << "  \"last_rebuild_result\":\""
         << kernel::core::json_escape(runtime_snapshot.last_rebuild_result) << "\",\n"
         << "  \"last_rebuild_at_ns\":" << runtime_snapshot.last_rebuild_at_ns << ",\n"
         << "  \"last_rebuild_duration_ms\":"
         << runtime_snapshot.last_rebuild_duration_ms << ",\n"
         << "  \"indexed_note_count\":" << runtime_snapshot.indexed_note_count << ",\n"
         << "  \"attachment_count\":" << attachment_snapshot.attachment_count << ",\n"
         << "  \"attachment_live_count\":" << attachment_snapshot.attachment_count << ",\n"
         << "  \"missing_attachment_count\":"
         << attachment_snapshot.missing_attachment_count << ",\n"
         << "  \"orphaned_attachment_count\":"
         << attachment_snapshot.orphaned_attachment_count << ",\n"
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
         << kernel::core::json_escape(runtime_snapshot.last_attachment_recount_reason)
         << "\",\n"
         << "  \"last_attachment_recount_at_ns\":"
         << runtime_snapshot.last_attachment_recount_at_ns << ",\n"
         << "  \"last_pdf_recount_reason\":\""
         << kernel::core::json_escape(runtime_snapshot.last_pdf_recount_reason)
         << "\",\n"
         << "  \"last_pdf_recount_at_ns\":"
         << runtime_snapshot.last_pdf_recount_at_ns << ",\n"
         << "  \"last_domain_recount_reason\":\""
         << kernel::core::json_escape(runtime_snapshot.last_domain_recount_reason)
         << "\",\n"
         << "  \"last_domain_recount_at_ns\":"
         << runtime_snapshot.last_domain_recount_at_ns << ",\n"
         << "  \"attachment_public_surface_revision\":\""
         << kernel::core::json_escape(
                kernel::core::attachment_api::kAttachmentPublicSurfaceRevision)
         << "\",\n"
         << "  \"attachment_metadata_contract_revision\":\""
         << kernel::core::json_escape(
                kernel::core::attachment_api::kAttachmentMetadataContractRevision)
         << "\",\n"
         << "  \"attachment_kind_mapping_revision\":\""
         << kernel::core::json_escape(
                kernel::core::attachment_api::kAttachmentKindMappingRevision)
         << "\",\n"
         << "  \"pdf_contract_revision\":\""
         << kernel::core::json_escape(kernel::pdf::kPdfContractRevision) << "\",\n"
         << "  \"pdf_extract_mode\":\""
         << kernel::core::json_escape(kernel::pdf::kPdfExtractMode) << "\",\n"
         << "  \"pdf_lookup_key_mode\":\""
         << kernel::core::json_escape(kernel::pdf::kPdfLookupKeyMode) << "\",\n"
         << "  \"pdf_anchor_mode\":\""
         << kernel::core::json_escape(kernel::pdf::kPdfAnchorMode) << "\",\n"
         << "  \"pdf_live_count\":" << pdf_snapshot.live_pdf_count << ",\n"
         << "  \"pdf_metadata_ready_count\":" << pdf_snapshot.ready_pdf_count << ",\n"
         << "  \"pdf_metadata_partial_count\":" << pdf_snapshot.partial_pdf_count << ",\n"
         << "  \"pdf_metadata_invalid_count\":" << pdf_snapshot.invalid_pdf_count << ",\n"
         << "  \"pdf_metadata_unavailable_count\":"
         << pdf_snapshot.unavailable_pdf_count << ",\n"
         << "  \"pdf_live_anchor_count\":" << pdf_snapshot.live_pdf_anchor_count << ",\n"
         << "  \"pdf_source_ref_count\":" << pdf_snapshot.pdf_source_ref_count << ",\n"
         << "  \"pdf_source_ref_resolved_count\":"
         << pdf_snapshot.resolved_pdf_source_ref_count << ",\n"
         << "  \"pdf_source_ref_missing_count\":"
         << pdf_snapshot.missing_pdf_source_ref_count << ",\n"
         << "  \"pdf_source_ref_stale_count\":"
         << pdf_snapshot.stale_pdf_source_ref_count << ",\n"
         << "  \"pdf_source_ref_unresolved_count\":"
         << pdf_snapshot.unresolved_pdf_source_ref_count << ",\n"
         << "  \"pdf_metadata_anomaly_summary\":\""
         << kernel::core::json_escape(build_pdf_metadata_anomaly_summary(pdf_snapshot))
         << "\",\n"
         << "  \"pdf_reference_anomaly_summary\":\""
         << kernel::core::json_escape(build_pdf_reference_anomaly_summary(pdf_snapshot))
         << "\",\n"
         << "  \"domain_contract_revision\":\""
         << kernel::core::json_escape(
                kernel::core::domain_contract::kDomainContractRevision)
         << "\",\n"
         << "  \"domain_diagnostics_revision\":\""
         << kernel::core::json_escape(
                kernel::core::domain_contract::kDomainDiagnosticsRevision)
         << "\",\n"
         << "  \"domain_benchmark_gate_revision\":\""
         << kernel::core::json_escape(
                kernel::core::domain_contract::kDomainBenchmarkGateRevision)
         << "\",\n"
         << "  \"domain_namespace_summary\":\""
         << kernel::core::json_escape(
                kernel::core::domain_contract::kDomainNamespaceSummary)
         << "\",\n"
         << "  \"domain_subtype_summary\":\""
         << kernel::core::json_escape(
                kernel::core::domain_contract::kDomainSubtypeSummary)
         << "\",\n"
         << "  \"domain_source_reference_summary\":\""
         << kernel::core::json_escape(
                kernel::core::domain_contract::kDomainSourceReferenceSummary)
         << "\",\n"
         << "  \"domain_attachment_metadata_entry_count\":"
         << domain_snapshot.attachment_domain_metadata_entry_count << ",\n"
         << "  \"domain_pdf_metadata_entry_count\":"
         << domain_snapshot.pdf_domain_metadata_entry_count << ",\n"
         << "  \"domain_object_count\":" << domain_snapshot.domain_object_count << ",\n"
         << "  \"domain_source_ref_count\":" << domain_snapshot.domain_source_ref_count
         << ",\n"
         << "  \"domain_source_ref_resolved_count\":"
         << domain_snapshot.resolved_domain_source_ref_count << ",\n"
         << "  \"domain_source_ref_missing_count\":"
         << domain_snapshot.missing_domain_source_ref_count << ",\n"
         << "  \"domain_source_ref_stale_count\":"
         << domain_snapshot.stale_domain_source_ref_count << ",\n"
         << "  \"domain_source_ref_unresolved_count\":"
         << domain_snapshot.unresolved_domain_source_ref_count << ",\n"
         << "  \"domain_source_ref_unsupported_count\":"
         << domain_snapshot.unsupported_domain_source_ref_count << ",\n"
         << "  \"domain_unresolved_summary\":\""
         << kernel::core::json_escape(build_domain_unresolved_summary(domain_snapshot))
         << "\",\n"
         << "  \"domain_stale_summary\":\""
         << kernel::core::json_escape(build_domain_stale_summary(domain_snapshot))
         << "\",\n"
         << "  \"domain_unsupported_summary\":\""
         << kernel::core::json_escape(build_domain_unsupported_summary(domain_snapshot))
         << "\",\n"
         << "  \"capability_track_status_summary\":\""
         << kernel::core::json_escape(
                kernel::core::domain_contract::kDomainCapabilityTrackStatusSummary)
         << "\",\n"
         << "  \"last_continuity_fallback_reason\":\""
         << kernel::core::json_escape(runtime_snapshot.last_continuity_fallback_reason)
         << "\",\n"
         << "  \"last_continuity_fallback_at_ns\":"
         << runtime_snapshot.last_continuity_fallback_at_ns << ",\n"
         << "  \"pending_recovery_ops\":" << runtime_snapshot.pending_recovery_ops << ",\n"
         << "  \"logger_backend\":\""
         << kernel::core::json_escape(runtime_snapshot.logger_backend) << "\",\n"
         << "  \"search_contract_revision\":\""
         << kernel::core::json_escape(kernel::search::kSearchContractRevision) << "\",\n"
         << "  \"search_backend\":\""
         << kernel::core::json_escape(kernel::search::kSearchBackend) << "\",\n"
         << "  \"search_snippet_mode\":\""
         << kernel::core::json_escape(kernel::search::kSearchSnippetMode) << "\",\n"
         << "  \"search_snippet_max_bytes\":" << kernel::search::kSearchSnippetMaxBytes
         << ",\n"
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
         << (kernel::search::kSearchTagExactBoostSingleTokenOnly ? "true" : "false")
         << ",\n"
         << "  \"search_all_kind_order\":\""
         << kernel::core::json_escape(kernel::search::kSearchAllKindOrder) << "\",\n"
         << "  \"vault_root\":\""
         << kernel::core::json_escape(runtime_snapshot.root.generic_string()) << "\",\n"
         << "  \"state_dir\":\""
         << kernel::core::json_escape(runtime_snapshot.state_dir.generic_string()) << "\",\n"
         << "  \"storage_db_path\":\""
         << kernel::core::json_escape(runtime_snapshot.storage_db_path.generic_string())
         << "\",\n"
         << "  \"recovery_journal_path\":\""
         << kernel::core::json_escape(
                runtime_snapshot.recovery_journal_path.generic_string())
         << "\"\n"
         << "}\n";
  return output.str();
}

}  // namespace kernel::core::diagnostics_export
