// Reason: This file owns the formal Track 5 Batch 1 chemistry metadata public
// surface so chemistry capability ABI stays separate from generic domain and
// attachment/PDF units.

#include "kernel/c_api.h"

#include "core/kernel_chemistry_api_shared.h"
#include "core/kernel_chemistry_query_shared.h"
#include "core/kernel_domain_api_shared.h"
#include "core/kernel_shared.h"

#include <vector>

namespace {

constexpr size_t kDefaultChemSpectraLimit = 512;

}  // namespace

extern "C" kernel_status kernel_query_chem_spectrum_metadata(
    kernel_handle* handle,
    const char* attachment_rel_path,
    const size_t limit,
    kernel_domain_metadata_list* out_entries) {
  kernel::core::domain_api::reset_domain_metadata_list(out_entries);
  if (handle == nullptr || out_entries == nullptr || limit == 0) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::vector<kernel::core::domain_api::DomainMetadataView> entries;
  const kernel_status query_status =
      kernel::core::chemistry_query::query_chem_spectrum_metadata(
          handle,
          attachment_rel_path,
          limit,
          entries);
  if (query_status.code != KERNEL_OK) {
    return query_status;
  }

  return kernel::core::domain_api::fill_domain_metadata_list(entries, out_entries);
}

extern "C" kernel_status kernel_query_chem_spectra(
    kernel_handle* handle,
    const size_t limit,
    kernel_chem_spectrum_list* out_spectra) {
  kernel::core::chemistry_api::reset_chem_spectrum_list(out_spectra);
  if (handle == nullptr || out_spectra == nullptr || limit == 0) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::vector<kernel::core::chemistry_api::ChemSpectrumView> spectra;
  const kernel_status query_status =
      kernel::core::chemistry_query::query_chem_spectra(
          handle,
          limit,
          spectra);
  if (query_status.code != KERNEL_OK) {
    return query_status;
  }

  return kernel::core::chemistry_api::fill_chem_spectrum_list(spectra, out_spectra);
}

extern "C" kernel_status kernel_get_chem_spectra_default_limit(size_t* out_limit) {
  if (out_limit == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  *out_limit = kDefaultChemSpectraLimit;
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_get_chem_spectrum(
    kernel_handle* handle,
    const char* attachment_rel_path,
    kernel_chem_spectrum_record* out_spectrum) {
  kernel::core::chemistry_api::reset_chem_spectrum_record(out_spectrum);
  if (handle == nullptr || out_spectrum == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel::core::chemistry_api::ChemSpectrumView spectrum;
  const kernel_status query_status =
      kernel::core::chemistry_query::query_chem_spectrum(
          handle,
          attachment_rel_path,
          spectrum);
  if (query_status.code != KERNEL_OK) {
    return query_status;
  }

  return kernel::core::chemistry_api::fill_chem_spectrum_record(spectrum, out_spectrum);
}
