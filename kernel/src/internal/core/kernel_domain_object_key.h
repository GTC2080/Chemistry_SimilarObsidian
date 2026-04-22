// Reason: This file owns Track 4 domain-object-key serialization so subtype
// queries and future domain source references reuse one canonical grammar.

#pragma once

#include "kernel/c_api.h"

#include <string>
#include <string_view>

namespace kernel::core::domain_object_key {

struct ParsedDomainObjectKey {
  kernel_domain_carrier_kind carrier_kind = KERNEL_DOMAIN_CARRIER_ATTACHMENT;
  std::string carrier_kind_token;
  std::string carrier_key;
  std::string subtype_namespace;
  std::string subtype_name;
};

std::string make_domain_object_key(
    kernel_domain_carrier_kind carrier_kind,
    std::string_view carrier_key,
    std::string_view subtype_namespace,
    std::string_view subtype_name);
bool parse_domain_object_key(
    std::string_view serialized_key,
    ParsedDomainObjectKey& out_key);

}  // namespace kernel::core::domain_object_key
