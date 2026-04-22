// Reason: This file centralizes Track 5 Batch 2 chemistry-spectrum ABI helpers
// so chemistry subtype result shaping stays separate from generic domain
// objects and metadata units.

#pragma once

#include "kernel/c_api.h"

#include <cstdint>
#include <string>
#include <vector>

namespace kernel::core::chemistry_api {

inline constexpr std::uint32_t kChemSpectrumSubtypeRevision = 1;

struct ChemSpectrumView {
  std::string attachment_rel_path;
  std::string domain_object_key;
  std::uint32_t subtype_revision = kChemSpectrumSubtypeRevision;
  kernel_chem_spectrum_format source_format = KERNEL_CHEM_SPECTRUM_FORMAT_UNKNOWN;
  kernel_attachment_kind coarse_kind = KERNEL_ATTACHMENT_KIND_UNKNOWN;
  kernel_attachment_presence presence = KERNEL_ATTACHMENT_PRESENCE_PRESENT;
  kernel_domain_object_state state = KERNEL_DOMAIN_OBJECT_UNRESOLVED;
  std::uint32_t flags = KERNEL_DOMAIN_OBJECT_FLAG_NONE;
};

void reset_chem_spectrum_record(kernel_chem_spectrum_record* out_spectrum);
void reset_chem_spectrum_list(kernel_chem_spectrum_list* out_spectra);
kernel_status fill_chem_spectrum_record(
    const ChemSpectrumView& spectrum,
    kernel_chem_spectrum_record* out_spectrum);
kernel_status fill_chem_spectrum_list(
    const std::vector<ChemSpectrumView>& spectra,
    kernel_chem_spectrum_list* out_spectra);

}  // namespace kernel::core::chemistry_api
