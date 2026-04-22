// Reason: This file owns frozen PDF reference-state resolution so Batch 3
// query surfaces and Batch 4 diagnostics export share one validation path.

#include "core/kernel_pdf_reference_resolution.h"

#include "pdf/pdf_anchor.h"

namespace kernel::core::pdf_reference_resolution {

std::error_code resolve_pdf_reference(
    kernel::storage::Database& storage,
    std::string_view pdf_rel_path,
    std::string_view anchor_serialized,
    const std::uint64_t stored_page,
    std::string_view stored_excerpt_text,
    ResolvedPdfReference& out_reference) {
  out_reference = ResolvedPdfReference{};
  out_reference.page = stored_page;
  out_reference.excerpt_text = std::string(stored_excerpt_text);

  kernel::storage::PdfMetadataRecord metadata;
  const std::error_code metadata_ec =
      kernel::storage::read_live_pdf_metadata_record(storage, pdf_rel_path, metadata);
  if (metadata_ec) {
    if (metadata_ec == std::make_error_code(std::errc::no_such_file_or_directory)) {
      return {};
    }
    return metadata_ec;
  }

  if (metadata.is_missing) {
    out_reference.state = KERNEL_PDF_REF_MISSING;
    return {};
  }

  kernel::pdf::ParsedPdfAnchor parsed_anchor;
  if (!kernel::pdf::parse_pdf_anchor(anchor_serialized, parsed_anchor) ||
      parsed_anchor.rel_path != pdf_rel_path) {
    return {};
  }

  if (out_reference.page == 0) {
    out_reference.page = parsed_anchor.page;
  }

  kernel::storage::PdfAnchorRecord current_anchor;
  const std::error_code anchor_ec =
      kernel::storage::read_live_pdf_anchor_record(
          storage,
          pdf_rel_path,
          parsed_anchor.page,
          current_anchor);
  const auto validation =
      kernel::pdf::validate_pdf_anchor(
          anchor_serialized,
          &metadata,
          anchor_ec ? nullptr : &current_anchor);
  switch (validation.state) {
    case kernel::pdf::PdfAnchorValidationState::Resolved:
      out_reference.state = KERNEL_PDF_REF_RESOLVED;
      if (out_reference.excerpt_text.empty()) {
        out_reference.excerpt_text = validation.current_anchor.excerpt_text;
      }
      break;
    case kernel::pdf::PdfAnchorValidationState::Stale:
      out_reference.state = KERNEL_PDF_REF_STALE;
      break;
    case kernel::pdf::PdfAnchorValidationState::Unavailable:
      out_reference.state = KERNEL_PDF_REF_MISSING;
      break;
    case kernel::pdf::PdfAnchorValidationState::Unverifiable:
    default:
      break;
  }

  return {};
}

}  // namespace kernel::core::pdf_reference_resolution
