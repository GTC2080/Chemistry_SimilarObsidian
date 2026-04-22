// Reason: This file centralizes chemistry reference ABI helpers so Track 5
// Batch 3 result shaping stays separate from generic domain and PDF ref units.

#pragma once

#include "kernel/c_api.h"

#include <cstdint>
#include <string>
#include <vector>

namespace kernel::core::chemistry_reference_api {

struct ChemSpectrumSourceRefView {
  std::string attachment_rel_path;
  std::string domain_object_key;
  kernel_chem_spectrum_selector_kind selector_kind =
      KERNEL_CHEM_SPECTRUM_SELECTOR_WHOLE_SPECTRUM;
  std::string selector_serialized;
  std::string preview_text;
  std::string target_basis_revision;
  kernel_domain_ref_state state = KERNEL_DOMAIN_REF_UNRESOLVED;
  std::uint32_t flags = KERNEL_DOMAIN_REF_FLAG_NONE;
};

struct ChemSpectrumReferrerView {
  std::string note_rel_path;
  std::string note_title;
  std::string attachment_rel_path;
  std::string domain_object_key;
  kernel_chem_spectrum_selector_kind selector_kind =
      KERNEL_CHEM_SPECTRUM_SELECTOR_WHOLE_SPECTRUM;
  std::string selector_serialized;
  std::string preview_text;
  std::string target_basis_revision;
  kernel_domain_ref_state state = KERNEL_DOMAIN_REF_UNRESOLVED;
  std::uint32_t flags = KERNEL_DOMAIN_REF_FLAG_NONE;
};

void reset_chem_spectrum_source_refs(kernel_chem_spectrum_source_refs* out_refs);
void reset_chem_spectrum_referrers(kernel_chem_spectrum_referrers* out_referrers);
kernel_status fill_chem_spectrum_source_refs(
    const std::vector<ChemSpectrumSourceRefView>& refs,
    kernel_chem_spectrum_source_refs* out_refs);
kernel_status fill_chem_spectrum_referrers(
    const std::vector<ChemSpectrumReferrerView>& referrers,
    kernel_chem_spectrum_referrers* out_referrers);

}  // namespace kernel::core::chemistry_reference_api
