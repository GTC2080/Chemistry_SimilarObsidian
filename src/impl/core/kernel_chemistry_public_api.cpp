// Reason: This file owns the formal Track 5 Batch 1 chemistry metadata public
// surface so chemistry capability ABI stays separate from generic domain and
// attachment/PDF units.

#include "kernel/c_api.h"

#include "core/kernel_chemistry_query_shared.h"
#include "core/kernel_domain_api_shared.h"
#include "core/kernel_shared.h"

#include <vector>

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
