// Reason: This file keeps Track 4 Batch 3 domain-reference query plumbing out
// of the public ABI wrapper so generic ref projection stays reusable.

#pragma once

#include "core/kernel_domain_reference_api_shared.h"
#include "core/kernel_internal.h"

#include <vector>

namespace kernel::core::domain_reference_query {

kernel_status query_note_domain_source_refs(
    kernel_handle* handle,
    const char* note_rel_path,
    size_t limit,
    std::vector<kernel::core::domain_reference_api::DomainSourceRefView>& out_refs);
kernel_status query_domain_object_referrers(
    kernel_handle* handle,
    const char* domain_object_key,
    size_t limit,
    std::vector<kernel::core::domain_reference_api::DomainReferrerView>& out_referrers);

}  // namespace kernel::core::domain_reference_query
