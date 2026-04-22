// Reason: This file centralizes Track 4 Batch 3 domain-reference ABI helpers
// so the generic source-reference surface stays separate from PDF-specific ABI.

#pragma once

#include "kernel/c_api.h"

#include <string>
#include <vector>

namespace kernel::core::domain_reference_api {

struct DomainSourceRefView {
  std::string target_object_key;
  kernel_domain_selector_kind selector_kind = KERNEL_DOMAIN_SELECTOR_OPAQUE_DOMAIN_SELECTOR;
  std::string selector_serialized;
  std::string preview_text;
  std::string target_basis_revision;
  kernel_domain_ref_state state = KERNEL_DOMAIN_REF_UNRESOLVED;
  std::uint32_t flags = KERNEL_DOMAIN_REF_FLAG_NONE;
};

struct DomainReferrerView {
  std::string note_rel_path;
  std::string note_title;
  std::string target_object_key;
  kernel_domain_selector_kind selector_kind = KERNEL_DOMAIN_SELECTOR_OPAQUE_DOMAIN_SELECTOR;
  std::string selector_serialized;
  std::string preview_text;
  std::string target_basis_revision;
  kernel_domain_ref_state state = KERNEL_DOMAIN_REF_UNRESOLVED;
  std::uint32_t flags = KERNEL_DOMAIN_REF_FLAG_NONE;
};

void reset_domain_source_refs(kernel_domain_source_refs* out_refs);
void reset_domain_referrers(kernel_domain_referrers* out_referrers);
kernel_status fill_domain_source_refs(
    const std::vector<DomainSourceRefView>& refs,
    kernel_domain_source_refs* out_refs);
kernel_status fill_domain_referrers(
    const std::vector<DomainReferrerView>& referrers,
    kernel_domain_referrers* out_referrers);

}  // namespace kernel::core::domain_reference_api
