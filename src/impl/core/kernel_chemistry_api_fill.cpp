// Reason: This file owns chemistry-spectrum ABI result marshalling so Track 5
// subtype queries can stay thin and focused.

#include "core/kernel_chemistry_api_shared.h"

#include "core/kernel_shared.h"

#include <new>

namespace kernel::core::chemistry_api {

kernel_status fill_chem_spectrum_record(
    const ChemSpectrumView& spectrum,
    kernel_chem_spectrum_record* out_spectrum) {
  out_spectrum->attachment_rel_path =
      kernel::core::duplicate_c_string(spectrum.attachment_rel_path);
  out_spectrum->domain_object_key =
      kernel::core::duplicate_c_string(spectrum.domain_object_key);
  out_spectrum->subtype_revision = spectrum.subtype_revision;
  out_spectrum->source_format = spectrum.source_format;
  out_spectrum->coarse_kind = spectrum.coarse_kind;
  out_spectrum->presence = spectrum.presence;
  out_spectrum->state = spectrum.state;
  out_spectrum->flags = spectrum.flags;

  if (out_spectrum->attachment_rel_path == nullptr ||
      out_spectrum->domain_object_key == nullptr) {
    reset_chem_spectrum_record(out_spectrum);
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }

  return kernel::core::make_status(KERNEL_OK);
}

kernel_status fill_chem_spectrum_list(
    const std::vector<ChemSpectrumView>& spectra,
    kernel_chem_spectrum_list* out_spectra) {
  if (spectra.empty()) {
    return kernel::core::make_status(KERNEL_OK);
  }

  auto* owned_spectra = new (std::nothrow) kernel_chem_spectrum_record[spectra.size()];
  if (owned_spectra == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }

  for (size_t index = 0; index < spectra.size(); ++index) {
    owned_spectra[index] = kernel_chem_spectrum_record{};
  }

  out_spectra->spectra = owned_spectra;
  out_spectra->count = spectra.size();
  for (size_t index = 0; index < spectra.size(); ++index) {
    const kernel_status status =
        fill_chem_spectrum_record(spectra[index], &owned_spectra[index]);
    if (status.code != KERNEL_OK) {
      reset_chem_spectrum_list(out_spectra);
      return status;
    }
  }

  return kernel::core::make_status(KERNEL_OK);
}

}  // namespace kernel::core::chemistry_api
