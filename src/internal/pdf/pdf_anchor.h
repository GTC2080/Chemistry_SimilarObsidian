// Reason: This file isolates the minimal Track 3 Batch 2 PDF anchor model so
// serialization, validation, and rebuildable page anchors stay out of ABI and
// storage plumbing.

#pragma once

#include "storage/storage.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace kernel::pdf {

inline constexpr std::string_view kPdfAnchorMode =
    "track3_batch2_page_excerpt_v1";
inline constexpr std::size_t kPdfAnchorExcerptMaxBytes = 96;

enum class PdfAnchorValidationState : std::uint8_t {
  Unavailable = 0,
  Resolved = 1,
  Stale = 2,
  Unverifiable = 3
};

struct ParsedPdfAnchor {
  std::string rel_path;
  std::string pdf_anchor_basis_revision;
  std::string excerpt_fingerprint;
  std::uint64_t page = 0;
};

struct PdfAnchorValidationResult {
  PdfAnchorValidationState state = PdfAnchorValidationState::Unavailable;
  ParsedPdfAnchor requested_anchor;
  kernel::storage::PdfMetadataRecord current_metadata;
  kernel::storage::PdfAnchorRecord current_anchor;
};

std::string build_pdf_anchor_basis_revision(
    std::string_view attachment_content_revision,
    std::string_view anchor_relevant_text_basis,
    std::string_view anchor_mode = kPdfAnchorMode);
std::string build_pdf_excerpt_fingerprint(std::string_view anchor_relevant_text_basis);
std::string serialize_pdf_anchor(const ParsedPdfAnchor& anchor);
bool parse_pdf_anchor(std::string_view serialized_anchor, ParsedPdfAnchor& out_anchor);
std::vector<kernel::storage::PdfAnchorRecord> extract_pdf_anchor_records(
    std::string_view rel_path,
    std::string_view bytes,
    std::string_view attachment_content_revision);
PdfAnchorValidationResult validate_pdf_anchor(
    std::string_view serialized_anchor,
    const kernel::storage::PdfMetadataRecord* current_metadata,
    const kernel::storage::PdfAnchorRecord* current_anchor);

}  // namespace kernel::pdf
