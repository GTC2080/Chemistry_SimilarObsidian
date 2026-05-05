// Reason: This file owns PDF note-reference query shaping so the Batch 3 ABI
// wrapper can stay focused on argument validation and marshalling.

#include "core/kernel_pdf_reference_query_shared.h"

#include "core/kernel_attachment_api_shared.h"
#include "core/kernel_pdf_reference_resolution.h"
#include "storage/storage.h"

namespace kernel::core::pdf_reference_query {
namespace {

kernel_status normalize_required_rel_path(
    const char* rel_path,
    std::string& out_rel_path) {
  return kernel::core::attachment_api::normalize_required_rel_path_argument(
      rel_path,
      out_rel_path);
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
    kernel::core::pdf_reference_resolution::ResolvedPdfReference resolved;
    const std::error_code resolve_ec =
        kernel::core::pdf_reference_resolution::resolve_pdf_reference(
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
    kernel::core::pdf_reference_resolution::ResolvedPdfReference resolved;
    const std::error_code resolve_ec =
        kernel::core::pdf_reference_resolution::resolve_pdf_reference(
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
