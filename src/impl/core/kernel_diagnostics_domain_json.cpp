// Reason: This file keeps Track 4 domain diagnostics JSON assembly separate so
// the shared diagnostics JSON builder does not bloat as new capability tracks land.

#include "core/kernel_diagnostics_support.h"

#include "core/kernel_shared.h"

#include <sstream>

namespace kernel::core::diagnostics_export {
namespace {

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

std::string build_domain_diagnostics_json_fragment(
    const RuntimeDiagnosticsSnapshot& runtime_snapshot,
    const DomainDiagnosticsSnapshot& domain_snapshot) {
  std::ostringstream output;
  output << "  \"last_domain_recount_reason\":\""
         << kernel::core::json_escape(runtime_snapshot.last_domain_recount_reason)
         << "\",\n"
         << "  \"last_domain_recount_at_ns\":"
         << runtime_snapshot.last_domain_recount_at_ns << ",\n"
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
         << "\",\n";
  return output.str();
}

}  // namespace kernel::core::diagnostics_export
