// Reason: This file owns the formal Track 4 Batch 3 generic source-reference
// ABI so domain refs do not bloat PDF-specific public-entry units.

#include "kernel/c_api.h"

#include "core/kernel_domain_reference_api_shared.h"
#include "core/kernel_domain_reference_query_shared.h"
#include "core/kernel_shared.h"

#include <vector>

extern "C" kernel_status kernel_query_note_domain_source_refs(
    kernel_handle* handle,
    const char* note_rel_path,
    const size_t limit,
    kernel_domain_source_refs* out_refs) {
  kernel::core::domain_reference_api::reset_domain_source_refs(out_refs);
  if (handle == nullptr || out_refs == nullptr || limit == 0) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::vector<kernel::core::domain_reference_api::DomainSourceRefView> refs;
  const kernel_status query_status =
      kernel::core::domain_reference_query::query_note_domain_source_refs(
          handle,
          note_rel_path,
          limit,
          refs);
  if (query_status.code != KERNEL_OK) {
    return query_status;
  }

  return kernel::core::domain_reference_api::fill_domain_source_refs(refs, out_refs);
}

extern "C" kernel_status kernel_query_domain_object_referrers(
    kernel_handle* handle,
    const char* domain_object_key,
    const size_t limit,
    kernel_domain_referrers* out_referrers) {
  kernel::core::domain_reference_api::reset_domain_referrers(out_referrers);
  if (handle == nullptr || out_referrers == nullptr || limit == 0) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::vector<kernel::core::domain_reference_api::DomainReferrerView> referrers;
  const kernel_status query_status =
      kernel::core::domain_reference_query::query_domain_object_referrers(
          handle,
          domain_object_key,
          limit,
          referrers);
  if (query_status.code != KERNEL_OK) {
    return query_status;
  }

  return kernel::core::domain_reference_api::fill_domain_referrers(
      referrers,
      out_referrers);
}
