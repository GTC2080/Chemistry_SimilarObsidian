// Reason: This file owns the formal Track 4 Batch 1 domain-metadata public
// surface so the new ABI stays separate from attachment- and PDF-specific
// units.

#include "kernel/c_api.h"

#include "core/kernel_domain_api_shared.h"
#include "core/kernel_domain_query_shared.h"
#include "core/kernel_shared.h"

#include <vector>

extern "C" kernel_status kernel_query_attachment_domain_metadata(
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
      kernel::core::domain_query::query_attachment_domain_metadata(
          handle,
          attachment_rel_path,
          limit,
          entries);
  if (query_status.code != KERNEL_OK) {
    return query_status;
  }

  return kernel::core::domain_api::fill_domain_metadata_list(entries, out_entries);
}

extern "C" kernel_status kernel_query_pdf_domain_metadata(
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
      kernel::core::domain_query::query_pdf_domain_metadata(
          handle,
          attachment_rel_path,
          limit,
          entries);
  if (query_status.code != KERNEL_OK) {
    return query_status;
  }

  return kernel::core::domain_api::fill_domain_metadata_list(entries, out_entries);
}
