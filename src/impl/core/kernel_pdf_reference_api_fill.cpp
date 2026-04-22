// Reason: This file owns PDF note-reference ABI marshalling so Track 3 Batch 3
// can evolve without inflating the metadata-only PDF ABI unit.

#include "core/kernel_pdf_reference_api_shared.h"

#include "core/kernel_shared.h"

#include <new>

namespace kernel::core::pdf_reference_api {

kernel_status fill_pdf_source_refs(
    const std::vector<kernel::core::pdf_reference_query::PdfSourceRefView>& refs,
    kernel_pdf_source_refs* out_refs) {
  if (refs.empty()) {
    return kernel::core::make_status(KERNEL_OK);
  }

  auto* owned_refs = new (std::nothrow) kernel_pdf_source_ref[refs.size()];
  if (owned_refs == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }

  for (size_t index = 0; index < refs.size(); ++index) {
    owned_refs[index] = kernel_pdf_source_ref{};
    owned_refs[index].state = KERNEL_PDF_REF_UNRESOLVED;
  }

  out_refs->refs = owned_refs;
  out_refs->count = refs.size();
  for (size_t index = 0; index < refs.size(); ++index) {
    owned_refs[index].pdf_rel_path = kernel::core::duplicate_c_string(refs[index].pdf_rel_path);
    owned_refs[index].anchor_serialized =
        kernel::core::duplicate_c_string(refs[index].anchor_serialized);
    owned_refs[index].page = refs[index].page;
    owned_refs[index].state = refs[index].state;
    if (!refs[index].excerpt_text.empty()) {
      owned_refs[index].excerpt_text =
          kernel::core::duplicate_c_string(refs[index].excerpt_text);
    }
    if (owned_refs[index].pdf_rel_path == nullptr ||
        owned_refs[index].anchor_serialized == nullptr ||
        (!refs[index].excerpt_text.empty() && owned_refs[index].excerpt_text == nullptr)) {
      reset_pdf_source_refs(out_refs);
      return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
    }
  }

  return kernel::core::make_status(KERNEL_OK);
}

kernel_status fill_pdf_referrers(
    const std::vector<kernel::core::pdf_reference_query::PdfReferrerView>& referrers,
    kernel_pdf_referrers* out_referrers) {
  if (referrers.empty()) {
    return kernel::core::make_status(KERNEL_OK);
  }

  auto* owned_referrers = new (std::nothrow) kernel_pdf_referrer[referrers.size()];
  if (owned_referrers == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }

  for (size_t index = 0; index < referrers.size(); ++index) {
    owned_referrers[index] = kernel_pdf_referrer{};
    owned_referrers[index].state = KERNEL_PDF_REF_UNRESOLVED;
  }

  out_referrers->referrers = owned_referrers;
  out_referrers->count = referrers.size();
  for (size_t index = 0; index < referrers.size(); ++index) {
    owned_referrers[index].note_rel_path =
        kernel::core::duplicate_c_string(referrers[index].note_rel_path);
    owned_referrers[index].note_title =
        kernel::core::duplicate_c_string(referrers[index].note_title);
    owned_referrers[index].anchor_serialized =
        kernel::core::duplicate_c_string(referrers[index].anchor_serialized);
    owned_referrers[index].page = referrers[index].page;
    owned_referrers[index].state = referrers[index].state;
    if (!referrers[index].excerpt_text.empty()) {
      owned_referrers[index].excerpt_text =
          kernel::core::duplicate_c_string(referrers[index].excerpt_text);
    }
    if (owned_referrers[index].note_rel_path == nullptr ||
        owned_referrers[index].note_title == nullptr ||
        owned_referrers[index].anchor_serialized == nullptr ||
        (!referrers[index].excerpt_text.empty() &&
         owned_referrers[index].excerpt_text == nullptr)) {
      reset_pdf_referrers(out_referrers);
      return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
    }
  }

  return kernel::core::make_status(KERNEL_OK);
}

}  // namespace kernel::core::pdf_reference_api
