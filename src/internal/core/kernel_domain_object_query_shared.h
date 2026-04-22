// Reason: This file keeps Track 4 Batch 2 subtype query plumbing out of the
// public ABI wrappers so object identity and projection rules stay reusable.

#pragma once

#include "core/kernel_domain_object_api_shared.h"
#include "core/kernel_internal.h"

#include <vector>

namespace kernel::core::domain_object_query {

kernel_status query_attachment_domain_objects(
    kernel_handle* handle,
    const char* attachment_rel_path,
    size_t limit,
    std::vector<kernel::core::domain_object_api::DomainObjectView>& out_objects);
kernel_status query_pdf_domain_objects(
    kernel_handle* handle,
    const char* attachment_rel_path,
    size_t limit,
    std::vector<kernel::core::domain_object_api::DomainObjectView>& out_objects);
kernel_status query_domain_object(
    kernel_handle* handle,
    const char* domain_object_key,
    kernel::core::domain_object_api::DomainObjectView& out_object);

}  // namespace kernel::core::domain_object_query
