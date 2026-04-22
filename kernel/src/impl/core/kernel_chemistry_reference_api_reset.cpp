// Reason: This file owns chemistry reference ABI cleanup so query and
// marshalling code do not also carry reset/free logic.

#include "core/kernel_chemistry_reference_api_shared.h"

namespace {

void free_source_ref_impl(kernel_chem_spectrum_source_ref* ref) {
  if (ref == nullptr) {
    return;
  }

  delete[] ref->attachment_rel_path;
  delete[] ref->domain_object_key;
  delete[] ref->selector_serialized;
  delete[] ref->preview_text;
  delete[] ref->target_basis_revision;
  ref->attachment_rel_path = nullptr;
  ref->domain_object_key = nullptr;
  ref->selector_kind = KERNEL_CHEM_SPECTRUM_SELECTOR_WHOLE_SPECTRUM;
  ref->selector_serialized = nullptr;
  ref->preview_text = nullptr;
  ref->target_basis_revision = nullptr;
  ref->state = KERNEL_DOMAIN_REF_UNRESOLVED;
  ref->flags = KERNEL_DOMAIN_REF_FLAG_NONE;
}

void free_referrer_impl(kernel_chem_spectrum_referrer* referrer) {
  if (referrer == nullptr) {
    return;
  }

  delete[] referrer->note_rel_path;
  delete[] referrer->note_title;
  delete[] referrer->attachment_rel_path;
  delete[] referrer->domain_object_key;
  delete[] referrer->selector_serialized;
  delete[] referrer->preview_text;
  delete[] referrer->target_basis_revision;
  referrer->note_rel_path = nullptr;
  referrer->note_title = nullptr;
  referrer->attachment_rel_path = nullptr;
  referrer->domain_object_key = nullptr;
  referrer->selector_kind = KERNEL_CHEM_SPECTRUM_SELECTOR_WHOLE_SPECTRUM;
  referrer->selector_serialized = nullptr;
  referrer->preview_text = nullptr;
  referrer->target_basis_revision = nullptr;
  referrer->state = KERNEL_DOMAIN_REF_UNRESOLVED;
  referrer->flags = KERNEL_DOMAIN_REF_FLAG_NONE;
}

}  // namespace

namespace kernel::core::chemistry_reference_api {

void reset_chem_spectrum_source_refs(kernel_chem_spectrum_source_refs* out_refs) {
  if (out_refs == nullptr) {
    return;
  }

  if (out_refs->refs != nullptr) {
    for (size_t index = 0; index < out_refs->count; ++index) {
      free_source_ref_impl(&out_refs->refs[index]);
    }
    delete[] out_refs->refs;
  }

  out_refs->refs = nullptr;
  out_refs->count = 0;
}

void reset_chem_spectrum_referrers(kernel_chem_spectrum_referrers* out_referrers) {
  if (out_referrers == nullptr) {
    return;
  }

  if (out_referrers->referrers != nullptr) {
    for (size_t index = 0; index < out_referrers->count; ++index) {
      free_referrer_impl(&out_referrers->referrers[index]);
    }
    delete[] out_referrers->referrers;
  }

  out_referrers->referrers = nullptr;
  out_referrers->count = 0;
}

}  // namespace kernel::core::chemistry_reference_api

extern "C" void kernel_free_chem_spectrum_source_refs(kernel_chem_spectrum_source_refs* refs) {
  kernel::core::chemistry_reference_api::reset_chem_spectrum_source_refs(refs);
}

extern "C" void kernel_free_chem_spectrum_referrers(kernel_chem_spectrum_referrers* referrers) {
  kernel::core::chemistry_reference_api::reset_chem_spectrum_referrers(referrers);
}
