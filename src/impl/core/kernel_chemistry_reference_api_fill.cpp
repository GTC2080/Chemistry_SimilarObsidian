// Reason: This file owns chemistry reference ABI result marshalling so Track
// 5 Batch 3 query wrappers can stay thin and focused.

#include "core/kernel_chemistry_reference_api_shared.h"

#include "core/kernel_shared.h"

#include <new>

namespace {

kernel_status fill_source_ref(
    const kernel::core::chemistry_reference_api::ChemSpectrumSourceRefView& ref,
    kernel_chem_spectrum_source_ref* out_ref) {
  out_ref->attachment_rel_path =
      kernel::core::duplicate_c_string(ref.attachment_rel_path);
  out_ref->domain_object_key =
      kernel::core::duplicate_c_string(ref.domain_object_key);
  out_ref->selector_kind = ref.selector_kind;
  out_ref->selector_serialized =
      kernel::core::duplicate_c_string(ref.selector_serialized);
  out_ref->preview_text = kernel::core::duplicate_c_string(ref.preview_text);
  out_ref->target_basis_revision =
      kernel::core::duplicate_c_string(ref.target_basis_revision);
  out_ref->state = ref.state;
  out_ref->flags = ref.flags;

  if (out_ref->attachment_rel_path == nullptr || out_ref->domain_object_key == nullptr ||
      out_ref->selector_serialized == nullptr || out_ref->preview_text == nullptr ||
      out_ref->target_basis_revision == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

kernel_status fill_referrer(
    const kernel::core::chemistry_reference_api::ChemSpectrumReferrerView& referrer,
    kernel_chem_spectrum_referrer* out_referrer) {
  out_referrer->note_rel_path =
      kernel::core::duplicate_c_string(referrer.note_rel_path);
  out_referrer->note_title =
      kernel::core::duplicate_c_string(referrer.note_title);
  out_referrer->attachment_rel_path =
      kernel::core::duplicate_c_string(referrer.attachment_rel_path);
  out_referrer->domain_object_key =
      kernel::core::duplicate_c_string(referrer.domain_object_key);
  out_referrer->selector_kind = referrer.selector_kind;
  out_referrer->selector_serialized =
      kernel::core::duplicate_c_string(referrer.selector_serialized);
  out_referrer->preview_text =
      kernel::core::duplicate_c_string(referrer.preview_text);
  out_referrer->target_basis_revision =
      kernel::core::duplicate_c_string(referrer.target_basis_revision);
  out_referrer->state = referrer.state;
  out_referrer->flags = referrer.flags;

  if (out_referrer->note_rel_path == nullptr || out_referrer->note_title == nullptr ||
      out_referrer->attachment_rel_path == nullptr ||
      out_referrer->domain_object_key == nullptr ||
      out_referrer->selector_serialized == nullptr ||
      out_referrer->preview_text == nullptr ||
      out_referrer->target_basis_revision == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

}  // namespace

namespace kernel::core::chemistry_reference_api {

kernel_status fill_chem_spectrum_source_refs(
    const std::vector<ChemSpectrumSourceRefView>& refs,
    kernel_chem_spectrum_source_refs* out_refs) {
  if (refs.empty()) {
    return kernel::core::make_status(KERNEL_OK);
  }

  auto* owned_refs = new (std::nothrow) kernel_chem_spectrum_source_ref[refs.size()];
  if (owned_refs == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }

  for (size_t index = 0; index < refs.size(); ++index) {
    owned_refs[index] = kernel_chem_spectrum_source_ref{};
  }

  out_refs->refs = owned_refs;
  out_refs->count = refs.size();
  for (size_t index = 0; index < refs.size(); ++index) {
    const kernel_status status = fill_source_ref(refs[index], &owned_refs[index]);
    if (status.code != KERNEL_OK) {
      reset_chem_spectrum_source_refs(out_refs);
      return status;
    }
  }
  return kernel::core::make_status(KERNEL_OK);
}

kernel_status fill_chem_spectrum_referrers(
    const std::vector<ChemSpectrumReferrerView>& referrers,
    kernel_chem_spectrum_referrers* out_referrers) {
  if (referrers.empty()) {
    return kernel::core::make_status(KERNEL_OK);
  }

  auto* owned_referrers =
      new (std::nothrow) kernel_chem_spectrum_referrer[referrers.size()];
  if (owned_referrers == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }

  for (size_t index = 0; index < referrers.size(); ++index) {
    owned_referrers[index] = kernel_chem_spectrum_referrer{};
  }

  out_referrers->referrers = owned_referrers;
  out_referrers->count = referrers.size();
  for (size_t index = 0; index < referrers.size(); ++index) {
    const kernel_status status = fill_referrer(referrers[index], &owned_referrers[index]);
    if (status.code != KERNEL_OK) {
      reset_chem_spectrum_referrers(out_referrers);
      return status;
    }
  }
  return kernel::core::make_status(KERNEL_OK);
}

}  // namespace kernel::core::chemistry_reference_api
