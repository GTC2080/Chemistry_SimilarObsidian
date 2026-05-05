// Reason: This file keeps the small search ABI validation and buffer helpers
// shared by the focused search public API implementation units.

#include "core/kernel_search_api_shared.h"

#include "core/kernel_shared.h"

#include <cctype>
#include <cstring>
#include <new>
#include <string_view>

namespace kernel::core::search_api {

bool is_null_or_whitespace_only(const char* value) {
  if (kernel::core::is_null_or_empty(value)) {
    return true;
  }

  for (const char* cursor = value; *cursor != '\0'; ++cursor) {
    if (!std::isspace(static_cast<unsigned char>(*cursor))) {
      return false;
    }
  }
  return true;
}

bool optional_search_field_uses_default(const char* value) {
  return value == nullptr || *value == '\0';
}

bool fill_owned_buffer(std::string_view value, kernel_owned_buffer* out_buffer) {
  if (out_buffer == nullptr) {
    return false;
  }

  *out_buffer = kernel_owned_buffer{};
  if (value.empty()) {
    return true;
  }

  auto* data = new (std::nothrow) char[value.size()];
  if (data == nullptr) {
    return false;
  }

  std::memcpy(data, value.data(), value.size());
  out_buffer->data = data;
  out_buffer->size = value.size();
  return true;
}

}  // namespace kernel::core::search_api
