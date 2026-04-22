// Reason: This file owns domain-metadata ABI result marshalling so cleanup and
// query plumbing can stay in smaller dedicated units.

#include "core/kernel_domain_api_shared.h"

#include "core/kernel_shared.h"

#include <new>

namespace kernel::core::domain_api {

kernel_status fill_domain_metadata_list(
    const std::vector<DomainMetadataView>& entries,
    kernel_domain_metadata_list* out_entries) {
  if (entries.empty()) {
    return kernel::core::make_status(KERNEL_OK);
  }

  auto* owned_entries = new (std::nothrow) kernel_domain_metadata_entry[entries.size()];
  if (owned_entries == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }

  for (size_t index = 0; index < entries.size(); ++index) {
    owned_entries[index] = kernel_domain_metadata_entry{};
  }

  out_entries->entries = owned_entries;
  out_entries->count = entries.size();
  for (size_t index = 0; index < entries.size(); ++index) {
    auto& owned = owned_entries[index];
    owned.carrier_kind = entries[index].carrier_kind;
    owned.carrier_key = kernel::core::duplicate_c_string(entries[index].carrier_key);
    owned.namespace_name = kernel::core::duplicate_c_string(entries[index].namespace_name);
    owned.public_schema_revision = entries[index].public_schema_revision;
    owned.key_name = kernel::core::duplicate_c_string(entries[index].key_name);
    owned.value_kind = entries[index].value_kind;
    owned.bool_value = entries[index].bool_value ? 1 : 0;
    owned.uint64_value = entries[index].uint64_value;
    owned.flags = entries[index].flags;
    if (!entries[index].string_value.empty()) {
      owned.string_value = kernel::core::duplicate_c_string(entries[index].string_value);
    }
    if (owned.carrier_key == nullptr || owned.namespace_name == nullptr ||
        owned.key_name == nullptr ||
        (!entries[index].string_value.empty() && owned.string_value == nullptr)) {
      reset_domain_metadata_list(out_entries);
      return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
    }
  }

  return kernel::core::make_status(KERNEL_OK);
}

}  // namespace kernel::core::domain_api
