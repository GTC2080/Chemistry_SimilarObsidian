// Reason: This file owns domain-object ABI result marshalling so subtype query
// wrappers can stay thin and focused.

#include "core/kernel_domain_object_api_shared.h"

#include "core/kernel_shared.h"

#include <new>

namespace kernel::core::domain_object_api {

kernel_status fill_domain_object_descriptor(
    const DomainObjectView& object,
    kernel_domain_object_descriptor* out_object) {
  out_object->domain_object_key = kernel::core::duplicate_c_string(object.domain_object_key);
  out_object->carrier_kind = object.carrier_kind;
  out_object->carrier_key = kernel::core::duplicate_c_string(object.carrier_key);
  out_object->subtype_namespace = kernel::core::duplicate_c_string(object.subtype_namespace);
  out_object->subtype_name = kernel::core::duplicate_c_string(object.subtype_name);
  out_object->subtype_revision = object.subtype_revision;
  out_object->coarse_kind = object.coarse_kind;
  out_object->presence = object.presence;
  out_object->state = object.state;
  out_object->flags = object.flags;

  if (out_object->domain_object_key == nullptr || out_object->carrier_key == nullptr ||
      out_object->subtype_namespace == nullptr || out_object->subtype_name == nullptr) {
    reset_domain_object_descriptor(out_object);
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }

  return kernel::core::make_status(KERNEL_OK);
}

kernel_status fill_domain_object_list(
    const std::vector<DomainObjectView>& objects,
    kernel_domain_object_list* out_objects) {
  if (objects.empty()) {
    return kernel::core::make_status(KERNEL_OK);
  }

  auto* owned_objects = new (std::nothrow) kernel_domain_object_descriptor[objects.size()];
  if (owned_objects == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }

  for (size_t index = 0; index < objects.size(); ++index) {
    owned_objects[index] = kernel_domain_object_descriptor{};
  }

  out_objects->objects = owned_objects;
  out_objects->count = objects.size();
  for (size_t index = 0; index < objects.size(); ++index) {
    const kernel_status status =
        fill_domain_object_descriptor(objects[index], &owned_objects[index]);
    if (status.code != KERNEL_OK) {
      reset_domain_object_list(out_objects);
      return status;
    }
  }

  return kernel::core::make_status(KERNEL_OK);
}

}  // namespace kernel::core::domain_object_api
