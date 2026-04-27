// Reason: This file owns the formal Track 5 Batch 3 chemistry ref public
// surface so chemistry source refs stay separate from generic domain and PDF
// ref entry units.

#include "kernel/c_api.h"

#include "core/kernel_chemistry_reference_api_shared.h"
#include "core/kernel_chemistry_reference_query_shared.h"
#include "core/kernel_shared.h"

#include <vector>

namespace {

constexpr size_t kDefaultChemSpectrumRefsLimit = 512;
constexpr size_t kDefaultChemSpectrumReferrersLimit = 512;

}  // namespace

extern "C" kernel_status kernel_query_note_chem_spectrum_refs(
    kernel_handle* handle,
    const char* note_rel_path,
    const size_t limit,
    kernel_chem_spectrum_source_refs* out_refs) {
  kernel::core::chemistry_reference_api::reset_chem_spectrum_source_refs(out_refs);
  if (handle == nullptr || out_refs == nullptr || limit == 0) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::vector<kernel::core::chemistry_reference_api::ChemSpectrumSourceRefView> refs;
  const kernel_status query_status =
      kernel::core::chemistry_reference_query::query_note_chem_spectrum_refs(
          handle,
          note_rel_path,
          limit,
          refs);
  if (query_status.code != KERNEL_OK) {
    return query_status;
  }

  return kernel::core::chemistry_reference_api::fill_chem_spectrum_source_refs(refs, out_refs);
}

extern "C" kernel_status kernel_get_note_chem_spectrum_refs_default_limit(size_t* out_limit) {
  if (out_limit == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  *out_limit = kDefaultChemSpectrumRefsLimit;
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_query_chem_spectrum_referrers(
    kernel_handle* handle,
    const char* attachment_rel_path,
    const size_t limit,
    kernel_chem_spectrum_referrers* out_referrers) {
  kernel::core::chemistry_reference_api::reset_chem_spectrum_referrers(out_referrers);
  if (handle == nullptr || out_referrers == nullptr || limit == 0) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::vector<kernel::core::chemistry_reference_api::ChemSpectrumReferrerView> referrers;
  const kernel_status query_status =
      kernel::core::chemistry_reference_query::query_chem_spectrum_referrers(
          handle,
          attachment_rel_path,
          limit,
          referrers);
  if (query_status.code != KERNEL_OK) {
    return query_status;
  }

  return kernel::core::chemistry_reference_api::fill_chem_spectrum_referrers(
      referrers,
      out_referrers);
}

extern "C" kernel_status kernel_get_chem_spectrum_referrers_default_limit(size_t* out_limit) {
  if (out_limit == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  *out_limit = kDefaultChemSpectrumReferrersLimit;
  return kernel::core::make_status(KERNEL_OK);
}
