// Reason: This file isolates the minimal Track 3 PDF metadata extractor and
// revision rules so refresh and public API code can stay focused on lifecycle
// and marshalling instead of PDF token heuristics.

#pragma once

#include "storage/storage.h"

#include <string>
#include <string_view>

namespace kernel::pdf {

inline constexpr std::string_view kPdfContractRevision =
    "track3_batch1_pdf_metadata_v1";
inline constexpr std::string_view kPdfExtractMode =
    "track3_batch1_headless_minimal_parse_v1";
inline constexpr std::string_view kPdfLookupKeyMode =
    "normalized_live_attachment_rel_path";

bool is_pdf_rel_path(std::string_view rel_path);
std::string build_pdf_metadata_revision(std::string_view attachment_content_revision);
kernel::storage::PdfMetadataRecord extract_pdf_metadata(
    std::string_view rel_path,
    std::string_view bytes,
    std::string_view attachment_content_revision);

}  // namespace kernel::pdf
