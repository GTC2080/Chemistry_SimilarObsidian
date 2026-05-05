// Reason: Share tiny product C ABI allocation helpers across focused wrapper files.

#pragma once

#include "core/kernel_shared.h"
#include "kernel/c_api.h"

#include <cstring>
#include <new>
#include <string_view>

namespace kernel::core::product::api {

inline bool fill_owned_buffer(std::string_view value, kernel_owned_buffer* out_buffer) {
  out_buffer->data = nullptr;
  out_buffer->size = 0;
  if (value.empty()) {
    return true;
  }

  auto* owned = new (std::nothrow) char[value.size()];
  if (owned == nullptr) {
    return false;
  }
  std::memcpy(owned, value.data(), value.size());
  out_buffer->data = owned;
  out_buffer->size = value.size();
  return true;
}

inline kernel_status write_size_limit(std::size_t value, std::size_t* out_value) {
  if (out_value == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  *out_value = value;
  return kernel::core::make_status(KERNEL_OK);
}

}  // namespace kernel::core::product::api
