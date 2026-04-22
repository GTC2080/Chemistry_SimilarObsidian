// Reason: This file isolates the minimal Track 5 Batch 3 chemistry selector
// model so serialization, normalization, and basis validation stay out of ABI
// and storage units.

#pragma once

#include "chemistry/chemistry_spectrum_metadata.h"
#include "kernel/c_api.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace kernel::chemistry {

inline constexpr std::string_view kChemistrySelectorModeRevision =
    "track5_batch3_spectrum_selector_v1";

enum class ChemSpectrumSelectorKind : std::uint8_t {
  WholeSpectrum = 0,
  XRange = 1
};

struct ParsedChemSpectrumSelector {
  ChemSpectrumSelectorKind kind = ChemSpectrumSelectorKind::WholeSpectrum;
  std::string chemistry_selector_basis_revision;
  std::string start;
  std::string end;
  std::string unit;
};

std::string build_normalized_spectrum_basis(const SpectrumMetadata& metadata);
std::string build_chemistry_selector_basis_revision(
    std::string_view attachment_content_revision,
    std::string_view normalized_spectrum_basis,
    std::string_view selector_mode = kChemistrySelectorModeRevision);
bool normalize_selector_decimal(std::string_view raw_value, std::string& out_value);
std::string serialize_chem_spectrum_selector(const ParsedChemSpectrumSelector& selector);
bool parse_chem_spectrum_selector(
    std::string_view serialized_selector,
    ParsedChemSpectrumSelector& out_selector);
std::string build_chem_spectrum_selector_preview(const ParsedChemSpectrumSelector& selector);
kernel_chem_spectrum_selector_kind to_public_selector_kind(ChemSpectrumSelectorKind kind);

}  // namespace kernel::chemistry
