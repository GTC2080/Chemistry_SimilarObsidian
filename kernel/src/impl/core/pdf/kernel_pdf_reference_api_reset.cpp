// Reason: This file owns PDF note-reference ABI cleanup so marshalling and
// public entry points stay small and focused.

#include "core/kernel_pdf_reference_api_shared.h"

namespace {

void free_pdf_source_ref_impl(kernel_pdf_source_ref* ref) {
  if (ref == nullptr) {
    return;
  }

  delete[] ref->pdf_rel_path;
  delete[] ref->anchor_serialized;
  delete[] ref->excerpt_text;
  ref->pdf_rel_path = nullptr;
  ref->anchor_serialized = nullptr;
  ref->excerpt_text = nullptr;
  ref->page = 0;
  ref->state = KERNEL_PDF_REF_UNRESOLVED;
}

void free_pdf_referrer_impl(kernel_pdf_referrer* referrer) {
  if (referrer == nullptr) {
    return;
  }

  delete[] referrer->note_rel_path;
  delete[] referrer->note_title;
  delete[] referrer->anchor_serialized;
  delete[] referrer->excerpt_text;
  referrer->note_rel_path = nullptr;
  referrer->note_title = nullptr;
  referrer->anchor_serialized = nullptr;
  referrer->excerpt_text = nullptr;
  referrer->page = 0;
  referrer->state = KERNEL_PDF_REF_UNRESOLVED;
}

}  // namespace

namespace kernel::core::pdf_reference_api {

void reset_pdf_source_refs(kernel_pdf_source_refs* out_refs) {
  if (out_refs == nullptr) {
    return;
  }

  if (out_refs->refs != nullptr) {
    for (size_t index = 0; index < out_refs->count; ++index) {
      free_pdf_source_ref_impl(&out_refs->refs[index]);
    }
    delete[] out_refs->refs;
  }

  out_refs->refs = nullptr;
  out_refs->count = 0;
}

void reset_pdf_referrers(kernel_pdf_referrers* out_referrers) {
  if (out_referrers == nullptr) {
    return;
  }

  if (out_referrers->referrers != nullptr) {
    for (size_t index = 0; index < out_referrers->count; ++index) {
      free_pdf_referrer_impl(&out_referrers->referrers[index]);
    }
    delete[] out_referrers->referrers;
  }

  out_referrers->referrers = nullptr;
  out_referrers->count = 0;
}

}  // namespace kernel::core::pdf_reference_api

extern "C" void kernel_free_pdf_source_refs(kernel_pdf_source_refs* refs) {
  kernel::core::pdf_reference_api::reset_pdf_source_refs(refs);
}

extern "C" void kernel_free_pdf_referrers(kernel_pdf_referrers* referrers) {
  kernel::core::pdf_reference_api::reset_pdf_referrers(referrers);
}
