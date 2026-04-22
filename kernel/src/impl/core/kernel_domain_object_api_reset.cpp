// Reason: This file owns domain-object ABI cleanup so query and marshalling
// code do not also carry reset/free logic.

#include "core/kernel_domain_object_api_shared.h"

namespace {

void free_domain_object_descriptor_impl(kernel_domain_object_descriptor* object) {
  if (object == nullptr) {
    return;
  }

  delete[] object->domain_object_key;
  delete[] object->carrier_key;
  delete[] object->subtype_namespace;
  delete[] object->subtype_name;
  object->domain_object_key = nullptr;
  object->carrier_kind = KERNEL_DOMAIN_CARRIER_ATTACHMENT;
  object->carrier_key = nullptr;
  object->subtype_namespace = nullptr;
  object->subtype_name = nullptr;
  object->subtype_revision = 0;
  object->coarse_kind = KERNEL_ATTACHMENT_KIND_UNKNOWN;
  object->presence = KERNEL_ATTACHMENT_PRESENCE_PRESENT;
  object->state = KERNEL_DOMAIN_OBJECT_UNRESOLVED;
  object->flags = KERNEL_DOMAIN_OBJECT_FLAG_NONE;
}

}  // namespace

namespace kernel::core::domain_object_api {

void reset_domain_object_descriptor(kernel_domain_object_descriptor* out_object) {
  free_domain_object_descriptor_impl(out_object);
}

void reset_domain_object_list(kernel_domain_object_list* out_objects) {
  if (out_objects == nullptr) {
    return;
  }

  if (out_objects->objects != nullptr) {
    for (size_t index = 0; index < out_objects->count; ++index) {
      free_domain_object_descriptor_impl(&out_objects->objects[index]);
    }
    delete[] out_objects->objects;
  }

  out_objects->objects = nullptr;
  out_objects->count = 0;
}

}  // namespace kernel::core::domain_object_api

extern "C" void kernel_free_domain_object_descriptor(kernel_domain_object_descriptor* object) {
  kernel::core::domain_object_api::reset_domain_object_descriptor(object);
}

extern "C" void kernel_free_domain_object_list(kernel_domain_object_list* objects) {
  kernel::core::domain_object_api::reset_domain_object_list(objects);
}
