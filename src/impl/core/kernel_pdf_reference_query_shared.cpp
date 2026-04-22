// Reason: This file owns PDF note-reference query shaping so the Batch 3 ABI
// wrapper can stay focused on argument validation and marshalling.

#include "core/kernel_pdf_reference_query_shared.h"

#include "core/kernel_attachment_api_shared.h"
#include "pdf/pdf_anchor.h"
#include "storage/storage.h"

namespace kernel::core::pdf_reference_query {
namespace {

struct ResolvedPdfReference {
  std::string excerpt_text;
  std::uint64_t page = 0;
  kernel_pdf_ref_state state = KERNEL_PDF_REF_UNRESOLVED;
};

kernel_status normalize_required_rel_path(
    const char* rel_path,
    std::string& out_rel_path) {
  return kernel::core::attachment_api::normalize_required_rel_path_argument(
      rel_path,
      out_rel_path);
}

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

}  // namespace

kernel_status query_note_pdf_source_refs(
    kernel_handle* handle,
    const char* note_rel_path,
    const size_t limit,
    std::vector<PdfSourceRefView>& out_refs) {
  std::string normalized_note_rel_path;
  const kernel_status normalized_status =
      normalize_required_rel_path(note_rel_path, normalized_note_rel_path);
  if (normalized_status.code != KERNEL_OK) {
    return normalized_status;
  }

  std::lock_guard lock(handle->storage_mutex);
  std::vector<kernel::storage::NotePdfSourceRefRecord> raw_records;
  const std::error_code query_ec =
      kernel::storage::list_note_pdf_source_ref_records(
          handle->storage,
          normalized_note_rel_path,
          limit,
          raw_records);
  if (query_ec) {
    return kernel::core::make_status(kernel::core::map_error(query_ec));
  }

  out_refs.clear();
  out_refs.reserve(raw_records.size());
  for (const auto& raw_record : raw_records) {
    ResolvedPdfReference resolved;
    const std::error_code resolve_ec =
        resolve_pdf_reference(
            handle->storage,
            raw_record.pdf_rel_path,
            raw_record.anchor_serialized,
            raw_record.page,
            raw_record.excerpt_text,
            resolved);
    if (resolve_ec) {
      return kernel::core::make_status(kernel::core::map_error(resolve_ec));
    }

    out_refs.push_back(
        PdfSourceRefView{
            raw_record.pdf_rel_path,
            raw_record.anchor_serialized,
            std::move(resolved.excerpt_text),
            resolved.page,
            resolved.state});
  }

  return kernel::core::make_status(KERNEL_OK);
}

kernel_status query_pdf_referrers(
    kernel_handle* handle,
    const char* attachment_rel_path,
    const size_t limit,
    std::vector<PdfReferrerView>& out_referrers) {
  std::string normalized_pdf_rel_path;
  const kernel_status normalized_status =
      normalize_required_rel_path(attachment_rel_path, normalized_pdf_rel_path);
  if (normalized_status.code != KERNEL_OK) {
    return normalized_status;
  }

  std::lock_guard lock(handle->storage_mutex);
  kernel::storage::PdfMetadataRecord metadata;
  const std::error_code metadata_ec =
      kernel::storage::read_live_pdf_metadata_record(
          handle->storage,
          normalized_pdf_rel_path,
          metadata);
  if (metadata_ec) {
    return kernel::core::make_status(kernel::core::map_error(metadata_ec));
  }

  std::vector<kernel::storage::PdfSourceReferrerRecord> raw_records;
  const std::error_code query_ec =
      kernel::storage::list_pdf_source_referrer_records(
          handle->storage,
          normalized_pdf_rel_path,
          limit,
          raw_records);
  if (query_ec) {
    return kernel::core::make_status(kernel::core::map_error(query_ec));
  }

  out_referrers.clear();
  out_referrers.reserve(raw_records.size());
  for (const auto& raw_record : raw_records) {
    ResolvedPdfReference resolved;
    const std::error_code resolve_ec =
        resolve_pdf_reference(
            handle->storage,
            normalized_pdf_rel_path,
            raw_record.anchor_serialized,
            raw_record.page,
            raw_record.excerpt_text,
            resolved);
    if (resolve_ec) {
      return kernel::core::make_status(kernel::core::map_error(resolve_ec));
    }

    out_referrers.push_back(
        PdfReferrerView{
            raw_record.note_rel_path,
            raw_record.note_title,
            raw_record.anchor_serialized,
            std::move(resolved.excerpt_text),
            resolved.page,
            resolved.state});
  }

  return kernel::core::make_status(KERNEL_OK);
}

}  // namespace kernel::core::pdf_reference_query
