// Reason: This file owns the formal Track 3 PDF metadata public surface so
// PDF ABI, cleanup, and marshalling stay out of attachment-focused units.

#include "kernel/c_api.h"

#include "core/kernel_pdf_api_shared.h"
#include "core/kernel_pdf_query_shared.h"
#include "core/kernel_shared.h"

namespace {

kernel_pdf_metadata_state to_public_metadata_state(
    const kernel::storage::PdfMetadataState state) {
  switch (state) {
    case kernel::storage::PdfMetadataState::Ready:
      return KERNEL_PDF_METADATA_READY;
    case kernel::storage::PdfMetadataState::Partial:
      return KERNEL_PDF_METADATA_PARTIAL;
    case kernel::storage::PdfMetadataState::Invalid:
      return KERNEL_PDF_METADATA_INVALID;
    case kernel::storage::PdfMetadataState::Unavailable:
    default:
      return KERNEL_PDF_METADATA_UNAVAILABLE;
  }
}

kernel_pdf_doc_title_state to_public_doc_title_state(
    const kernel::storage::PdfDocTitleState state) {
  switch (state) {
    case kernel::storage::PdfDocTitleState::Absent:
      return KERNEL_PDF_DOC_TITLE_ABSENT;
    case kernel::storage::PdfDocTitleState::Available:
      return KERNEL_PDF_DOC_TITLE_AVAILABLE;
    case kernel::storage::PdfDocTitleState::Unavailable:
    default:
      return KERNEL_PDF_DOC_TITLE_UNAVAILABLE;
  }
}

kernel_pdf_text_layer_state to_public_text_layer_state(
    const kernel::storage::PdfTextLayerState state) {
  switch (state) {
    case kernel::storage::PdfTextLayerState::Absent:
      return KERNEL_PDF_TEXT_LAYER_ABSENT;
    case kernel::storage::PdfTextLayerState::Present:
      return KERNEL_PDF_TEXT_LAYER_PRESENT;
    case kernel::storage::PdfTextLayerState::Unavailable:
    default:
      return KERNEL_PDF_TEXT_LAYER_UNAVAILABLE;
  }
}

void free_pdf_metadata_record_impl(kernel_pdf_metadata_record* metadata) {
  if (metadata == nullptr) {
    return;
  }

  delete[] metadata->rel_path;
  delete[] metadata->doc_title;
  delete[] metadata->pdf_metadata_revision;
  metadata->rel_path = nullptr;
  metadata->doc_title = nullptr;
  metadata->pdf_metadata_revision = nullptr;
  metadata->page_count = 0;
  metadata->has_outline = 0;
  metadata->presence = KERNEL_ATTACHMENT_PRESENCE_PRESENT;
  metadata->metadata_state = KERNEL_PDF_METADATA_UNAVAILABLE;
  metadata->doc_title_state = KERNEL_PDF_DOC_TITLE_UNAVAILABLE;
  metadata->text_layer_state = KERNEL_PDF_TEXT_LAYER_UNAVAILABLE;
}

}  // namespace

namespace kernel::core::pdf_api {

void reset_pdf_metadata_record(kernel_pdf_metadata_record* out_metadata) {
  free_pdf_metadata_record_impl(out_metadata);
}

kernel_status fill_pdf_metadata_record(
    const kernel::storage::PdfMetadataRecord& record,
    kernel_pdf_metadata_record* out_metadata) {
  out_metadata->rel_path = kernel::core::duplicate_c_string(record.rel_path);
  out_metadata->page_count = record.page_count;
  out_metadata->has_outline = record.has_outline ? 1 : 0;
  out_metadata->presence = record.is_missing ? KERNEL_ATTACHMENT_PRESENCE_MISSING
                                             : KERNEL_ATTACHMENT_PRESENCE_PRESENT;
  out_metadata->metadata_state = to_public_metadata_state(record.metadata_state);
  out_metadata->doc_title_state = to_public_doc_title_state(record.doc_title_state);
  out_metadata->text_layer_state = to_public_text_layer_state(record.text_layer_state);

  if (!record.doc_title.empty() &&
      record.doc_title_state == kernel::storage::PdfDocTitleState::Available) {
    out_metadata->doc_title = kernel::core::duplicate_c_string(record.doc_title);
  }
  if (!record.pdf_metadata_revision.empty()) {
    out_metadata->pdf_metadata_revision =
        kernel::core::duplicate_c_string(record.pdf_metadata_revision);
  }

  if (out_metadata->rel_path == nullptr ||
      (!record.doc_title.empty() && out_metadata->doc_title == nullptr) ||
      (!record.pdf_metadata_revision.empty() &&
       out_metadata->pdf_metadata_revision == nullptr)) {
    reset_pdf_metadata_record(out_metadata);
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }

  return kernel::core::make_status(KERNEL_OK);
}

}  // namespace kernel::core::pdf_api

extern "C" kernel_status kernel_get_pdf_metadata(
    kernel_handle* handle,
    const char* attachment_rel_path,
    kernel_pdf_metadata_record* out_metadata) {
  kernel::core::pdf_api::reset_pdf_metadata_record(out_metadata);
  if (handle == nullptr || out_metadata == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel::storage::PdfMetadataRecord record;
  const kernel_status query_status =
      kernel::core::pdf_query::query_live_pdf_metadata_record(
          handle,
          attachment_rel_path,
          record);
  if (query_status.code != KERNEL_OK) {
    return query_status;
  }

  return kernel::core::pdf_api::fill_pdf_metadata_record(record, out_metadata);
}

extern "C" void kernel_free_pdf_metadata_record(kernel_pdf_metadata_record* metadata) {
  kernel::core::pdf_api::reset_pdf_metadata_record(metadata);
}
