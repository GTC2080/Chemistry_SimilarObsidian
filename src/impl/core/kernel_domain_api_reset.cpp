// Reason: This file owns domain-metadata ABI cleanup so query and marshalling
// code do not also carry free/reset logic.

#include "core/kernel_domain_api_shared.h"

namespace {

void free_domain_metadata_entry_impl(kernel_domain_metadata_entry* entry) {
  if (entry == nullptr) {
    return;
  }

  delete[] entry->carrier_key;
  delete[] entry->namespace_name;
  delete[] entry->key_name;
  delete[] entry->string_value;
  entry->carrier_kind = KERNEL_DOMAIN_CARRIER_ATTACHMENT;
  entry->carrier_key = nullptr;
  entry->namespace_name = nullptr;
  entry->public_schema_revision = 0;
  entry->key_name = nullptr;
  entry->value_kind = KERNEL_DOMAIN_VALUE_TOKEN;
  entry->bool_value = 0;
  entry->uint64_value = 0;
  entry->string_value = nullptr;
  entry->flags = KERNEL_DOMAIN_METADATA_FLAG_NONE;
}

}  // namespace

namespace kernel::core::domain_api {

void reset_domain_metadata_list(kernel_domain_metadata_list* out_entries) {
  if (out_entries == nullptr) {
    return;
  }

  if (out_entries->entries != nullptr) {
    for (size_t index = 0; index < out_entries->count; ++index) {
      free_domain_metadata_entry_impl(&out_entries->entries[index]);
    }
    delete[] out_entries->entries;
  }

  out_entries->entries = nullptr;
  out_entries->count = 0;
}

}  // namespace kernel::core::domain_api

extern "C" void kernel_free_domain_metadata_list(kernel_domain_metadata_list* entries) {
  kernel::core::domain_api::reset_domain_metadata_list(entries);
}
