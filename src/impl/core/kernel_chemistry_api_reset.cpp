// Reason: This file owns chemistry-spectrum ABI cleanup so query and
// marshalling code do not also carry reset/free logic.

#include "core/kernel_chemistry_api_shared.h"

namespace {

void free_chem_spectrum_record_impl(kernel_chem_spectrum_record* spectrum) {
  if (spectrum == nullptr) {
    return;
  }

  delete[] spectrum->attachment_rel_path;
  delete[] spectrum->domain_object_key;
  spectrum->attachment_rel_path = nullptr;
  spectrum->domain_object_key = nullptr;
  spectrum->subtype_revision = 0;
  spectrum->source_format = KERNEL_CHEM_SPECTRUM_FORMAT_UNKNOWN;
  spectrum->coarse_kind = KERNEL_ATTACHMENT_KIND_UNKNOWN;
  spectrum->presence = KERNEL_ATTACHMENT_PRESENCE_PRESENT;
  spectrum->state = KERNEL_DOMAIN_OBJECT_UNRESOLVED;
  spectrum->flags = KERNEL_DOMAIN_OBJECT_FLAG_NONE;
}

}  // namespace

namespace kernel::core::chemistry_api {

void reset_chem_spectrum_record(kernel_chem_spectrum_record* out_spectrum) {
  free_chem_spectrum_record_impl(out_spectrum);
}

void reset_chem_spectrum_list(kernel_chem_spectrum_list* out_spectra) {
  if (out_spectra == nullptr) {
    return;
  }

  if (out_spectra->spectra != nullptr) {
    for (size_t index = 0; index < out_spectra->count; ++index) {
      free_chem_spectrum_record_impl(&out_spectra->spectra[index]);
    }
    delete[] out_spectra->spectra;
  }

  out_spectra->spectra = nullptr;
  out_spectra->count = 0;
}

}  // namespace kernel::core::chemistry_api

extern "C" void kernel_free_chem_spectrum_record(kernel_chem_spectrum_record* spectrum) {
  kernel::core::chemistry_api::reset_chem_spectrum_record(spectrum);
}

extern "C" void kernel_free_chem_spectrum_list(kernel_chem_spectrum_list* spectra) {
  kernel::core::chemistry_api::reset_chem_spectrum_list(spectra);
}
