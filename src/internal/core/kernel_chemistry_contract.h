// Reason: This file centralizes Track 5 public contract constants so
// chemistry diagnostics and future gates reuse one frozen vocabulary.

#pragma once

#include <string_view>

namespace kernel::core::chemistry_contract {

inline constexpr std::string_view kChemistryContractRevision =
    "track5_batch3_chemistry_source_reference_v1";
inline constexpr std::string_view kChemistryDiagnosticsRevision =
    "track5_batch4_chemistry_diagnostics_v1";
inline constexpr std::string_view kChemistryBenchmarkGateRevision =
    "track5_batch4_chemistry_query_gates_v1";

inline constexpr std::string_view kChemistryNamespaceSummary = "chem.spectrum";
inline constexpr std::string_view kChemistrySubtypeSummary = "chem.spectrum";
inline constexpr std::string_view kChemistrySourceReferenceSummary =
    "chem.spectrum_projection";
inline constexpr std::string_view kChemistryCapabilityTrackStatusSummary =
    "chemistry_metadata=gated;chemistry_spectra=gated;chemistry_refs=gated;chemistry_gates=gated";

}  // namespace kernel::core::chemistry_contract
