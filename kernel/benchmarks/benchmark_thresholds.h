// Reason: This file centralizes fixed benchmark baselines and regression thresholds so benchmark runs are comparable over time.

#pragma once

#include <cstdint>
#include <iostream>
#include <string_view>

namespace kernel::benchmarks {

struct BenchmarkGate {
  std::string_view name;
  std::int64_t baseline_ms;
  std::int64_t threshold_ms;
};

inline bool report_gate(const BenchmarkGate& gate, const std::int64_t observed_ms) {
  std::cout << " " << gate.name
            << "_baseline_ms=" << gate.baseline_ms
            << " " << gate.name
            << "_threshold_ms=" << gate.threshold_ms
            << " " << gate.name
            << "_observed_ms=" << observed_ms;
  return observed_ms <= gate.threshold_ms;
}

inline constexpr BenchmarkGate kStartupCleanGate{
    "clean",
    3465,
    6000};
inline constexpr BenchmarkGate kStartupRecoveryGate{
    "recovery",
    3865,
    7000};
inline constexpr BenchmarkGate kIoRoundtripGate{
    "io_roundtrip",
    168,
    500};
inline constexpr BenchmarkGate kExternalCreateGate{
    "external_create",
    289,
    800};
inline constexpr BenchmarkGate kRebuildGate{
    "rebuild",
    15885,
    22000};
inline constexpr BenchmarkGate kChemistryRebuildMixedSpectraDatasetGate{
    "chemistry_rebuild_mixed_spectra_dataset",
    19612,
    26000};
inline constexpr BenchmarkGate kTagQueryGate{
    "tag_query",
    143,
    250};
inline constexpr BenchmarkGate kTitleQueryGate{
    "title_query",
    88,
    140};
inline constexpr BenchmarkGate kBodyQueryGate{
    "body_query",
    77,
    140};
inline constexpr BenchmarkGate kBodySnippetQueryGate{
    "body_snippet_query",
    66,
    140};
inline constexpr BenchmarkGate kTitleOnlyQueryGate{
    "title_only_query",
    61,
    140};
inline constexpr BenchmarkGate kShallowPageQueryGate{
    "shallow_page_query",
    139,
    220};
inline constexpr BenchmarkGate kDeepOffsetQueryGate{
    "deep_offset_query",
    123,
    180};
inline constexpr BenchmarkGate kFilteredNoteQueryGate{
    "filtered_note_query",
    124,
    180};
inline constexpr BenchmarkGate kAttachmentPathQueryGate{
    "attachment_path_query",
    57,
    160};
inline constexpr BenchmarkGate kAttachmentCatalogQueryGate{
    "attachment_catalog_query",
    32,
    140};
inline constexpr BenchmarkGate kAttachmentLookupQueryGate{
    "attachment_lookup_query",
    18,
    120};
inline constexpr BenchmarkGate kNoteAttachmentRefsQueryGate{
    "note_attachment_refs_query",
    32,
    140};
inline constexpr BenchmarkGate kAttachmentReferrersQueryGate{
    "attachment_referrers_query",
    17,
    160};
inline constexpr BenchmarkGate kPdfSourceRefsQueryGate{
    "pdf_source_refs_query",
    100,
    180};
inline constexpr BenchmarkGate kPdfReferrersQueryGate{
    "pdf_referrers_query",
    162,
    220};
inline constexpr BenchmarkGate kDomainAttachmentMetadataQueryGate{
    "domain_attachment_metadata_query",
    29,
    140};
inline constexpr BenchmarkGate kDomainPdfMetadataQueryGate{
    "domain_pdf_metadata_query",
    45,
    160};
inline constexpr BenchmarkGate kDomainAttachmentObjectsQueryGate{
    "domain_attachment_objects_query",
    25,
    120};
inline constexpr BenchmarkGate kDomainPdfObjectsQueryGate{
    "domain_pdf_objects_query",
    29,
    140};
inline constexpr BenchmarkGate kDomainObjectLookupQueryGate{
    "domain_object_lookup_query",
    31,
    120};
inline constexpr BenchmarkGate kDomainNoteSourceRefsQueryGate{
    "domain_note_source_refs_query",
    117,
    220};
inline constexpr BenchmarkGate kDomainObjectReferrersQueryGate{
    "domain_object_referrers_query",
    256,
    340};
inline constexpr BenchmarkGate kChemistryMetadataQueryGate{
    "chemistry_metadata_query",
    99,
    180};
inline constexpr BenchmarkGate kChemistrySpectrumCatalogQueryGate{
    "chemistry_spectrum_catalog_query",
    264,
    380};
inline constexpr BenchmarkGate kChemistrySpectrumLookupQueryGate{
    "chemistry_spectrum_lookup_query",
    93,
    160};
inline constexpr BenchmarkGate kChemistryNoteSpectrumRefsQueryGate{
    "chemistry_note_spectrum_refs_query",
    233,
    320};
inline constexpr BenchmarkGate kChemistrySpectrumReferrersQueryGate{
    "chemistry_spectrum_referrers_query",
    438,
    620};
inline constexpr BenchmarkGate kAllKindQueryGate{
    "all_kind_query",
    167,
    240};
inline constexpr BenchmarkGate kRankedTitleQueryGate{
    "ranked_title_query",
    51,
    180};
inline constexpr BenchmarkGate kRankedTagBoostQueryGate{
    "ranked_tag_boost_query",
    51,
    180};
inline constexpr BenchmarkGate kRankedAllKindQueryGate{
    "ranked_all_kind_query",
    225,
    300};
inline constexpr BenchmarkGate kBacklinkQueryGate{
    "backlink_query",
    107,
    250};

}  // namespace kernel::benchmarks
