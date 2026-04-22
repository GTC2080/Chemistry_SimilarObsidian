// Reason: This file keeps Track 5 chemistry diagnostics JSON assembly
// separate so support-bundle growth does not bloat the shared JSON builder.

#include "core/kernel_diagnostics_support.h"

#include "core/kernel_chemistry_contract.h"
#include "core/kernel_shared.h"

#include <sstream>

namespace kernel::core::diagnostics_export {
namespace {

std::string build_chemistry_unresolved_summary(
    const ChemistryDiagnosticsSnapshot& snapshot) {
  const bool has_unresolved_spectra =
      snapshot.chemistry_spectra_unresolved_count != 0;
  const bool has_unresolved_refs =
      snapshot.chemistry_source_ref_unresolved_count != 0;
  if (has_unresolved_spectra && has_unresolved_refs) {
    return "chemistry_spectra_and_source_refs";
  }
  if (has_unresolved_spectra) {
    return "chemistry_spectra";
  }
  if (has_unresolved_refs) {
    return "chemistry_source_refs";
  }
  return "clean";
}

std::string build_chemistry_stale_summary(
    const ChemistryDiagnosticsSnapshot& snapshot) {
  return snapshot.chemistry_source_ref_stale_count != 0
             ? "chemistry_source_refs"
             : "clean";
}

std::string build_chemistry_unsupported_summary(
    const ChemistryDiagnosticsSnapshot& snapshot) {
  const bool has_unsupported_spectra =
      snapshot.chemistry_spectra_unsupported_count != 0;
  const bool has_unsupported_refs =
      snapshot.chemistry_source_ref_unsupported_count != 0;
  if (has_unsupported_spectra && has_unsupported_refs) {
    return "chemistry_spectra_and_source_refs";
  }
  if (has_unsupported_spectra) {
    return "chemistry_spectra";
  }
  if (has_unsupported_refs) {
    return "chemistry_source_refs";
  }
  return "clean";
}

}  // namespace

std::string build_chemistry_diagnostics_json_fragment(
    const RuntimeDiagnosticsSnapshot& runtime_snapshot,
    const ChemistryDiagnosticsSnapshot& chemistry_snapshot) {
  std::ostringstream output;
  output << "  \"last_chemistry_recount_reason\":\""
         << kernel::core::json_escape(runtime_snapshot.last_chemistry_recount_reason)
         << "\",\n"
         << "  \"last_chemistry_recount_at_ns\":"
         << runtime_snapshot.last_chemistry_recount_at_ns << ",\n"
         << "  \"chemistry_contract_revision\":\""
         << kernel::core::json_escape(
                kernel::core::chemistry_contract::kChemistryContractRevision)
         << "\",\n"
         << "  \"chemistry_diagnostics_revision\":\""
         << kernel::core::json_escape(
                kernel::core::chemistry_contract::kChemistryDiagnosticsRevision)
         << "\",\n"
         << "  \"chemistry_benchmark_gate_revision\":\""
         << kernel::core::json_escape(
                kernel::core::chemistry_contract::kChemistryBenchmarkGateRevision)
         << "\",\n"
         << "  \"chemistry_namespace_summary\":\""
         << kernel::core::json_escape(
                kernel::core::chemistry_contract::kChemistryNamespaceSummary)
         << "\",\n"
         << "  \"chemistry_spectra_subtype_summary\":\""
         << kernel::core::json_escape(
                kernel::core::chemistry_contract::kChemistrySubtypeSummary)
         << "\",\n"
         << "  \"chemistry_spectra_source_reference_summary\":\""
         << kernel::core::json_escape(
                kernel::core::chemistry_contract::kChemistrySourceReferenceSummary)
         << "\",\n"
         << "  \"chemistry_spectra_count\":"
         << chemistry_snapshot.chemistry_spectra_count << ",\n"
         << "  \"chemistry_spectra_present_count\":"
         << chemistry_snapshot.chemistry_spectra_present_count << ",\n"
         << "  \"chemistry_spectra_missing_count\":"
         << chemistry_snapshot.chemistry_spectra_missing_count << ",\n"
         << "  \"chemistry_spectra_unresolved_count\":"
         << chemistry_snapshot.chemistry_spectra_unresolved_count << ",\n"
         << "  \"chemistry_spectra_unsupported_count\":"
         << chemistry_snapshot.chemistry_spectra_unsupported_count << ",\n"
         << "  \"chemistry_source_ref_count\":"
         << chemistry_snapshot.chemistry_source_ref_count << ",\n"
         << "  \"chemistry_source_ref_resolved_count\":"
         << chemistry_snapshot.chemistry_source_ref_resolved_count << ",\n"
         << "  \"chemistry_source_ref_missing_count\":"
         << chemistry_snapshot.chemistry_source_ref_missing_count << ",\n"
         << "  \"chemistry_source_ref_stale_count\":"
         << chemistry_snapshot.chemistry_source_ref_stale_count << ",\n"
         << "  \"chemistry_source_ref_unresolved_count\":"
         << chemistry_snapshot.chemistry_source_ref_unresolved_count << ",\n"
         << "  \"chemistry_source_ref_unsupported_count\":"
         << chemistry_snapshot.chemistry_source_ref_unsupported_count << ",\n"
         << "  \"chemistry_unresolved_summary\":\""
         << kernel::core::json_escape(
                build_chemistry_unresolved_summary(chemistry_snapshot))
         << "\",\n"
         << "  \"chemistry_stale_summary\":\""
         << kernel::core::json_escape(
                build_chemistry_stale_summary(chemistry_snapshot))
         << "\",\n"
         << "  \"chemistry_unsupported_summary\":\""
         << kernel::core::json_escape(
                build_chemistry_unsupported_summary(chemistry_snapshot))
         << "\",\n"
         << "  \"chemistry_capability_track_status_summary\":\""
         << kernel::core::json_escape(
                kernel::core::chemistry_contract::kChemistryCapabilityTrackStatusSummary)
         << "\",\n";
  return output.str();
}

}  // namespace kernel::core::diagnostics_export
