// Reason: This file centralizes Track 4 public contract constants so domain
// metadata, subtype, source-reference, diagnostics, and benchmark code reuse
// one frozen vocabulary.

#pragma once

#include <cstddef>
#include <string_view>

namespace kernel::core::domain_contract {

inline constexpr std::string_view kDomainContractRevision =
    "track4_batch3_domain_extension_contract_v1";
inline constexpr std::string_view kDomainDiagnosticsRevision =
    "track4_batch4_domain_diagnostics_v1";
inline constexpr std::string_view kDomainBenchmarkGateRevision =
    "track4_batch4_domain_query_gates_v1";

inline constexpr std::size_t kAttachmentDomainMetadataPublicKeyCount = 3;
inline constexpr std::size_t kPdfDomainMetadataPublicKeyCount = 7;

inline constexpr std::string_view kDomainNamespaceSummary = "generic";
inline constexpr std::string_view kDomainSubtypeSummary =
    "generic.attachment_resource,generic.pdf_document";
inline constexpr std::string_view kDomainSourceReferenceSummary =
    "generic.pdf_document_projection";
inline constexpr std::string_view kDomainCapabilityTrackStatusSummary =
    "domain_metadata=gated;domain_objects=gated;domain_refs=gated";

}  // namespace kernel::core::domain_contract
