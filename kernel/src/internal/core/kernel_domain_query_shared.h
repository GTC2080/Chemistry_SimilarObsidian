// Reason: This file keeps Track 4 Batch 1 domain-metadata query plumbing out
// of the public ABI wrapper so the new surface stays small and focused.

#pragma once

#include "core/kernel_domain_api_shared.h"
#include "core/kernel_internal.h"

#include <vector>

namespace kernel::core::domain_query {

kernel_status query_attachment_domain_metadata(
    kernel_handle* handle,
    const char* attachment_rel_path,
    size_t limit,
    std::vector<kernel::core::domain_api::DomainMetadataView>& out_entries);
kernel_status query_pdf_domain_metadata(
    kernel_handle* handle,
    const char* attachment_rel_path,
    size_t limit,
    std::vector<kernel::core::domain_api::DomainMetadataView>& out_entries);

}  // namespace kernel::core::domain_query
