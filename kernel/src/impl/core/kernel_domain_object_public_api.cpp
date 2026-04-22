// Reason: This file owns the formal Track 4 Batch 2 subtype public surface so
// attachment and PDF units do not accumulate domain-object logic.

#include "kernel/c_api.h"

#include "core/kernel_domain_object_api_shared.h"
#include "core/kernel_domain_object_query_shared.h"
#include "core/kernel_shared.h"

#include <vector>

extern "C" kernel_status kernel_query_attachment_domain_objects(
    kernel_handle* handle,
    const char* attachment_rel_path,
    const size_t limit,
    kernel_domain_object_list* out_objects) {
  kernel::core::domain_object_api::reset_domain_object_list(out_objects);
  if (handle == nullptr || out_objects == nullptr || limit == 0) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::vector<kernel::core::domain_object_api::DomainObjectView> objects;
  const kernel_status query_status =
      kernel::core::domain_object_query::query_attachment_domain_objects(
          handle,
          attachment_rel_path,
          limit,
          objects);
  if (query_status.code != KERNEL_OK) {
    return query_status;
  }

  return kernel::core::domain_object_api::fill_domain_object_list(objects, out_objects);
}

extern "C" kernel_status kernel_query_pdf_domain_objects(
    kernel_handle* handle,
    const char* attachment_rel_path,
    const size_t limit,
    kernel_domain_object_list* out_objects) {
  kernel::core::domain_object_api::reset_domain_object_list(out_objects);
  if (handle == nullptr || out_objects == nullptr || limit == 0) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::vector<kernel::core::domain_object_api::DomainObjectView> objects;
  const kernel_status query_status =
      kernel::core::domain_object_query::query_pdf_domain_objects(
          handle,
          attachment_rel_path,
          limit,
          objects);
  if (query_status.code != KERNEL_OK) {
    return query_status;
  }

  return kernel::core::domain_object_api::fill_domain_object_list(objects, out_objects);
}

extern "C" kernel_status kernel_get_domain_object(
    kernel_handle* handle,
    const char* domain_object_key,
    kernel_domain_object_descriptor* out_object) {
  kernel::core::domain_object_api::reset_domain_object_descriptor(out_object);
  if (handle == nullptr || out_object == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel::core::domain_object_api::DomainObjectView object;
  const kernel_status query_status =
      kernel::core::domain_object_query::query_domain_object(
          handle,
          domain_object_key,
          object);
  if (query_status.code != KERNEL_OK) {
    return query_status;
  }

  return kernel::core::domain_object_api::fill_domain_object_descriptor(object, out_object);
}
