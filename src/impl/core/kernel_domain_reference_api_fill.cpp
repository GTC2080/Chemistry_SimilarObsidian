// Reason: This file owns generic domain-reference ABI marshalling so query
// wrappers can stay thin and focused.

#include "core/kernel_domain_reference_api_shared.h"

#include "core/kernel_shared.h"

#include <new>

namespace kernel::core::domain_reference_api {

kernel_status fill_domain_source_refs(
    const std::vector<DomainSourceRefView>& refs,
    kernel_domain_source_refs* out_refs) {
  if (refs.empty()) {
    return kernel::core::make_status(KERNEL_OK);
  }

  auto* owned_refs = new (std::nothrow) kernel_domain_source_ref[refs.size()];
  if (owned_refs == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }

  for (size_t index = 0; index < refs.size(); ++index) {
    owned_refs[index] = kernel_domain_source_ref{};
    owned_refs[index].selector_kind = KERNEL_DOMAIN_SELECTOR_OPAQUE_DOMAIN_SELECTOR;
    owned_refs[index].state = KERNEL_DOMAIN_REF_UNRESOLVED;
  }

  out_refs->refs = owned_refs;
  out_refs->count = refs.size();
  for (size_t index = 0; index < refs.size(); ++index) {
    owned_refs[index].target_object_key =
        kernel::core::duplicate_c_string(refs[index].target_object_key);
    owned_refs[index].selector_kind = refs[index].selector_kind;
    owned_refs[index].selector_serialized =
        kernel::core::duplicate_c_string(refs[index].selector_serialized);
    owned_refs[index].state = refs[index].state;
    owned_refs[index].flags = refs[index].flags;
    if (!refs[index].preview_text.empty()) {
      owned_refs[index].preview_text =
          kernel::core::duplicate_c_string(refs[index].preview_text);
    }
    if (!refs[index].target_basis_revision.empty()) {
      owned_refs[index].target_basis_revision =
          kernel::core::duplicate_c_string(refs[index].target_basis_revision);
    }
    if (owned_refs[index].target_object_key == nullptr ||
        owned_refs[index].selector_serialized == nullptr ||
        (!refs[index].preview_text.empty() && owned_refs[index].preview_text == nullptr) ||
        (!refs[index].target_basis_revision.empty() &&
         owned_refs[index].target_basis_revision == nullptr)) {
      reset_domain_source_refs(out_refs);
      return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
    }
  }

  return kernel::core::make_status(KERNEL_OK);
}

kernel_status fill_domain_referrers(
    const std::vector<DomainReferrerView>& referrers,
    kernel_domain_referrers* out_referrers) {
  if (referrers.empty()) {
    return kernel::core::make_status(KERNEL_OK);
  }

  auto* owned_referrers = new (std::nothrow) kernel_domain_referrer[referrers.size()];
  if (owned_referrers == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }

  for (size_t index = 0; index < referrers.size(); ++index) {
    owned_referrers[index] = kernel_domain_referrer{};
    owned_referrers[index].selector_kind = KERNEL_DOMAIN_SELECTOR_OPAQUE_DOMAIN_SELECTOR;
    owned_referrers[index].state = KERNEL_DOMAIN_REF_UNRESOLVED;
  }

  out_referrers->referrers = owned_referrers;
  out_referrers->count = referrers.size();
  for (size_t index = 0; index < referrers.size(); ++index) {
    owned_referrers[index].note_rel_path =
        kernel::core::duplicate_c_string(referrers[index].note_rel_path);
    owned_referrers[index].note_title =
        kernel::core::duplicate_c_string(referrers[index].note_title);
    owned_referrers[index].target_object_key =
        kernel::core::duplicate_c_string(referrers[index].target_object_key);
    owned_referrers[index].selector_kind = referrers[index].selector_kind;
    owned_referrers[index].selector_serialized =
        kernel::core::duplicate_c_string(referrers[index].selector_serialized);
    owned_referrers[index].state = referrers[index].state;
    owned_referrers[index].flags = referrers[index].flags;
    if (!referrers[index].preview_text.empty()) {
      owned_referrers[index].preview_text =
          kernel::core::duplicate_c_string(referrers[index].preview_text);
    }
    if (!referrers[index].target_basis_revision.empty()) {
      owned_referrers[index].target_basis_revision =
          kernel::core::duplicate_c_string(referrers[index].target_basis_revision);
    }
    if (owned_referrers[index].note_rel_path == nullptr ||
        owned_referrers[index].note_title == nullptr ||
        owned_referrers[index].target_object_key == nullptr ||
        owned_referrers[index].selector_serialized == nullptr ||
        (!referrers[index].preview_text.empty() &&
         owned_referrers[index].preview_text == nullptr) ||
        (!referrers[index].target_basis_revision.empty() &&
         owned_referrers[index].target_basis_revision == nullptr)) {
      reset_domain_referrers(out_referrers);
      return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
    }
  }

  return kernel::core::make_status(KERNEL_OK);
}

}  // namespace kernel::core::domain_reference_api
