// Reason: This file centralizes the diagnostics-export support types and
// helper declarations so collection, JSON building, and export flow can live
// in smaller focused implementation units.

#pragma once

#include "core/kernel_internal.h"
#include "core/kernel_domain_contract.h"
#include "kernel/c_api.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace kernel::core::diagnostics_export {

inline constexpr std::size_t kAttachmentAnomalyPathSummaryLimit = 16;

struct AttachmentDiagnosticsSnapshot {
  std::uint64_t attachment_count = 0;
  std::uint64_t missing_attachment_count = 0;
  std::uint64_t orphaned_attachment_count = 0;
  std::vector<std::string> missing_attachment_paths;
  std::vector<std::string> orphaned_attachment_paths;
};

struct PdfDiagnosticsSnapshot {
  std::uint64_t live_pdf_count = 0;
  std::uint64_t ready_pdf_count = 0;
  std::uint64_t partial_pdf_count = 0;
  std::uint64_t invalid_pdf_count = 0;
  std::uint64_t unavailable_pdf_count = 0;
  std::uint64_t live_pdf_anchor_count = 0;
  std::uint64_t pdf_source_ref_count = 0;
  std::uint64_t resolved_pdf_source_ref_count = 0;
  std::uint64_t missing_pdf_source_ref_count = 0;
  std::uint64_t stale_pdf_source_ref_count = 0;
  std::uint64_t unresolved_pdf_source_ref_count = 0;
};

struct DomainDiagnosticsSnapshot {
  std::uint64_t attachment_domain_metadata_entry_count = 0;
  std::uint64_t pdf_domain_metadata_entry_count = 0;
  std::uint64_t domain_object_count = 0;
  std::uint64_t domain_source_ref_count = 0;
  std::uint64_t resolved_domain_source_ref_count = 0;
  std::uint64_t missing_domain_source_ref_count = 0;
  std::uint64_t stale_domain_source_ref_count = 0;
  std::uint64_t unresolved_domain_source_ref_count = 0;
  std::uint64_t unsupported_domain_source_ref_count = 0;
};

struct ChemistryDiagnosticsSnapshot {
  std::uint64_t chemistry_spectra_count = 0;
  std::uint64_t chemistry_spectra_present_count = 0;
  std::uint64_t chemistry_spectra_missing_count = 0;
  std::uint64_t chemistry_spectra_unresolved_count = 0;
  std::uint64_t chemistry_spectra_unsupported_count = 0;
  std::uint64_t chemistry_source_ref_count = 0;
  std::uint64_t chemistry_source_ref_resolved_count = 0;
  std::uint64_t chemistry_source_ref_missing_count = 0;
  std::uint64_t chemistry_source_ref_stale_count = 0;
  std::uint64_t chemistry_source_ref_unresolved_count = 0;
  std::uint64_t chemistry_source_ref_unsupported_count = 0;
};

struct RuntimeDiagnosticsSnapshot {
  kernel_session_state session_state = KERNEL_SESSION_OPEN;
  kernel_index_state index_state = KERNEL_INDEX_UNAVAILABLE;
  bool rebuild_in_flight = false;
  bool has_last_rebuild_result = false;
  std::uint64_t rebuild_current_generation = 0;
  std::uint64_t rebuild_last_completed_generation = 0;
  std::uint64_t rebuild_current_started_at_ns = 0;
  std::string last_recovery_outcome;
  bool last_recovery_detected_corrupt_tail = false;
  std::uint64_t last_recovery_at_ns = 0;
  kernel_error_code last_rebuild_result_code = KERNEL_ERROR_NOT_FOUND;
  std::string index_fault_reason;
  int index_fault_code = 0;
  std::uint64_t index_fault_at_ns = 0;
  std::vector<KernelFaultRecord> fault_history;
  std::vector<KernelRecentEvent> recent_events;
  std::string last_rebuild_result;
  std::uint64_t last_rebuild_at_ns = 0;
  std::uint64_t last_rebuild_duration_ms = 0;
  std::uint64_t indexed_note_count = 0;
  std::string last_attachment_recount_reason;
  std::uint64_t last_attachment_recount_at_ns = 0;
  std::string last_pdf_recount_reason;
  std::uint64_t last_pdf_recount_at_ns = 0;
  std::string last_domain_recount_reason;
  std::uint64_t last_domain_recount_at_ns = 0;
  std::string last_chemistry_recount_reason;
  std::uint64_t last_chemistry_recount_at_ns = 0;
  std::string last_continuity_fallback_reason;
  std::uint64_t last_continuity_fallback_at_ns = 0;
  std::uint64_t pending_recovery_ops = 0;
  std::string logger_backend = "null_logger";
  std::filesystem::path root;
  std::filesystem::path state_dir;
  std::filesystem::path storage_db_path;
  std::filesystem::path recovery_journal_path;
};

kernel_status collect_attachment_diagnostics_snapshot(
    kernel_handle* handle,
    kernel_index_state index_state,
    AttachmentDiagnosticsSnapshot& out_snapshot);

kernel_status collect_pdf_diagnostics_snapshot(
    kernel_handle* handle,
    kernel_index_state index_state,
    PdfDiagnosticsSnapshot& out_snapshot);
kernel_status collect_chemistry_diagnostics_snapshot(
    kernel_handle* handle,
    kernel_index_state index_state,
    ChemistryDiagnosticsSnapshot& out_snapshot);
void collect_domain_diagnostics_snapshot(
    const AttachmentDiagnosticsSnapshot& attachment_snapshot,
    const PdfDiagnosticsSnapshot& pdf_snapshot,
    DomainDiagnosticsSnapshot& out_snapshot);

std::string build_diagnostics_json(
    const RuntimeDiagnosticsSnapshot& runtime_snapshot,
    const AttachmentDiagnosticsSnapshot& attachment_snapshot,
    const PdfDiagnosticsSnapshot& pdf_snapshot,
    const ChemistryDiagnosticsSnapshot& chemistry_snapshot,
    const DomainDiagnosticsSnapshot& domain_snapshot);

std::string build_domain_diagnostics_json_fragment(
    const RuntimeDiagnosticsSnapshot& runtime_snapshot,
    const DomainDiagnosticsSnapshot& domain_snapshot);
std::string build_chemistry_diagnostics_json_fragment(
    const RuntimeDiagnosticsSnapshot& runtime_snapshot,
    const ChemistryDiagnosticsSnapshot& chemistry_snapshot);

}  // namespace kernel::core::diagnostics_export
