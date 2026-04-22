// Reason: This file isolates chemistry reference-state resolution so public
// query surfaces and generic domain projection can share one frozen validation
// path.

#pragma once

#include "kernel/c_api.h"

#include <string>
#include <string_view>
#include <system_error>

namespace kernel::core::chemistry_reference_resolution {

struct ResolvedChemSpectrumReference {
  std::string preview_text;
  std::string target_basis_revision;
  kernel_chem_spectrum_selector_kind selector_kind =
      KERNEL_CHEM_SPECTRUM_SELECTOR_WHOLE_SPECTRUM;
  kernel_domain_ref_state state = KERNEL_DOMAIN_REF_UNRESOLVED;
};

std::error_code resolve_chem_spectrum_reference(
    kernel_handle* handle,
    std::string_view attachment_rel_path,
    std::string_view selector_serialized,
    std::string_view stored_preview_text,
    ResolvedChemSpectrumReference& out_reference);

}  // namespace kernel::core::chemistry_reference_resolution
