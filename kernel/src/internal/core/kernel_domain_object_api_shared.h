// Reason: This file centralizes Track 4 Batch 2 domain-object ABI helpers so
// the new subtype surface stays thin and separate from metadata units.

#pragma once

#include "kernel/c_api.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace kernel::core::domain_object_api {

inline constexpr std::uint32_t kGenericAttachmentSubtypeRevision = 1;
inline constexpr std::uint32_t kGenericPdfSubtypeRevision = 1;

struct DomainObjectView {
  std::string domain_object_key;
  kernel_domain_carrier_kind carrier_kind = KERNEL_DOMAIN_CARRIER_ATTACHMENT;
  std::string carrier_key;
  std::string subtype_namespace;
  std::string subtype_name;
  std::uint32_t subtype_revision = 0;
  kernel_attachment_kind coarse_kind = KERNEL_ATTACHMENT_KIND_UNKNOWN;
  kernel_attachment_presence presence = KERNEL_ATTACHMENT_PRESENCE_PRESENT;
  kernel_domain_object_state state = KERNEL_DOMAIN_OBJECT_UNRESOLVED;
  std::uint32_t flags = KERNEL_DOMAIN_OBJECT_FLAG_NONE;
};

void reset_domain_object_descriptor(kernel_domain_object_descriptor* out_object);
void reset_domain_object_list(kernel_domain_object_list* out_objects);
kernel_status fill_domain_object_descriptor(
    const DomainObjectView& object,
    kernel_domain_object_descriptor* out_object);
kernel_status fill_domain_object_list(
    const std::vector<DomainObjectView>& objects,
    kernel_domain_object_list* out_objects);

}  // namespace kernel::core::domain_object_api
