// Reason: This file centralizes Track 4 Batch 1 domain-metadata ABI helpers so
// the new substrate stays thin and separate from attachment and PDF units.

#pragma once

#include "kernel/c_api.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace kernel::core::domain_api {

inline constexpr std::string_view kDomainExtensionContractRevision =
    "track4_batch1_domain_extension_contract_v1";
inline constexpr std::uint32_t kGenericNamespaceRevision = 1;

struct DomainMetadataView {
  kernel_domain_carrier_kind carrier_kind = KERNEL_DOMAIN_CARRIER_ATTACHMENT;
  std::string carrier_key;
  std::string namespace_name;
  std::uint32_t public_schema_revision = 0;
  std::string key_name;
  kernel_domain_value_kind value_kind = KERNEL_DOMAIN_VALUE_TOKEN;
  bool bool_value = false;
  std::uint64_t uint64_value = 0;
  std::string string_value;
  std::uint32_t flags = KERNEL_DOMAIN_METADATA_FLAG_NONE;
};

void reset_domain_metadata_list(kernel_domain_metadata_list* out_entries);
kernel_status fill_domain_metadata_list(
    const std::vector<DomainMetadataView>& entries,
    kernel_domain_metadata_list* out_entries);

}  // namespace kernel::core::domain_api
