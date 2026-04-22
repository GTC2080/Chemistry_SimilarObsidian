// Reason: This file owns frozen chemistry reference-state resolution so Batch
// 3 query surfaces and generic domain projection share one validation path.

#include "core/kernel_chemistry_reference_resolution.h"

#include "chemistry/chemistry_spectrum_selector.h"
#include "core/kernel_chemistry_query_shared.h"

namespace kernel::core::chemistry_reference_resolution {

std::error_code resolve_chem_spectrum_reference(
    kernel_handle* handle,
    std::string_view attachment_rel_path,
    std::string_view selector_serialized,
    std::string_view stored_preview_text,
    ResolvedChemSpectrumReference& out_reference) {
  out_reference = ResolvedChemSpectrumReference{};
  out_reference.preview_text = std::string(stored_preview_text);

  kernel::chemistry::ParsedChemSpectrumSelector selector;
  if (!kernel::chemistry::parse_chem_spectrum_selector(selector_serialized, selector)) {
    return {};
  }

  out_reference.selector_kind =
      kernel::chemistry::to_public_selector_kind(selector.kind);
  out_reference.target_basis_revision = selector.chemistry_selector_basis_revision;
  if (out_reference.preview_text.empty()) {
    out_reference.preview_text =
        kernel::chemistry::build_chem_spectrum_selector_preview(selector);
  }

  kernel::core::chemistry_api::ChemSpectrumView spectrum;
  const kernel_status spectrum_status =
      kernel::core::chemistry_query::query_chem_spectrum(
          handle,
          std::string(attachment_rel_path).c_str(),
          spectrum);
  if (spectrum_status.code != KERNEL_OK) {
    out_reference.state = spectrum_status.code == KERNEL_ERROR_NOT_FOUND
                              ? KERNEL_DOMAIN_REF_MISSING
                              : KERNEL_DOMAIN_REF_UNRESOLVED;
    return {};
  }

  switch (spectrum.state) {
    case KERNEL_DOMAIN_OBJECT_MISSING:
      out_reference.state = KERNEL_DOMAIN_REF_MISSING;
      return {};
    case KERNEL_DOMAIN_OBJECT_UNSUPPORTED:
      out_reference.state = KERNEL_DOMAIN_REF_UNSUPPORTED;
      return {};
    case KERNEL_DOMAIN_OBJECT_UNRESOLVED:
      out_reference.state = KERNEL_DOMAIN_REF_UNRESOLVED;
      return {};
    case KERNEL_DOMAIN_OBJECT_PRESENT:
    default:
      break;
  }

  out_reference.state =
      selector.chemistry_selector_basis_revision == spectrum.selector_basis_revision
          ? KERNEL_DOMAIN_REF_RESOLVED
          : KERNEL_DOMAIN_REF_STALE;
  return {};
}

}  // namespace kernel::core::chemistry_reference_resolution
