// Reason: This file isolates the minimal Track 5 Batch 1 chemistry spectrum
// metadata extractor so chemistry parsing stays out of ABI wrappers and core
// domain plumbing.

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace kernel::chemistry {

inline constexpr std::string_view kChemistryContractRevision =
    "track5_batch1_chemistry_metadata_namespace_v1";
inline constexpr std::string_view kChemistryExtractModeRevision =
    "track5_batch1_spectra_metadata_extract_v1";
inline constexpr std::string_view kChemSpectrumNamespace = "chem.spectrum";
inline constexpr std::uint32_t kChemSpectrumNamespaceRevision = 1;
inline constexpr std::size_t kChemSpectrumSampleLabelMaxBytes = 128;

enum class SpectrumParseStatus : std::uint8_t {
  NotSupported = 0,
  Ready = 1,
  Unresolved = 2
};

struct SpectrumMetadata {
  std::string attachment_content_revision;
  std::string source_format;
  std::string family;
  std::string x_axis_unit;
  std::string y_axis_unit;
  std::uint64_t point_count = 0;
  std::string sample_label;
  std::string chemistry_metadata_revision;
};

struct SpectrumParseResult {
  SpectrumParseStatus status = SpectrumParseStatus::NotSupported;
  SpectrumMetadata metadata;
};

std::string build_chemistry_metadata_revision(std::string_view attachment_content_revision);
SpectrumParseResult extract_spectrum_metadata(
    std::string_view rel_path,
    std::string_view bytes,
    std::string_view attachment_content_revision);

}  // namespace kernel::chemistry
