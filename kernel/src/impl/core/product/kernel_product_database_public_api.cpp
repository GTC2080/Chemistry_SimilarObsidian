// Reason: Expose database product rules through focused C ABI wrappers.

#include "kernel/c_api.h"

#include "core/kernel_product_database.h"
#include "core/kernel_product_public_api_utils.h"
#include "core/kernel_shared.h"

#include <string>
#include <string_view>

extern "C" kernel_status kernel_normalize_database_column_type(
    const char* column_type,
    const std::size_t column_type_size,
    kernel_owned_buffer* out_buffer) {
  if (out_buffer == nullptr || (column_type_size > 0 && column_type == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  const std::string_view column_type_view(
      column_type == nullptr ? "" : column_type,
      column_type_size);
  const std::string normalized =
      kernel::core::product::normalize_database_column_type(column_type_view);
  if (!kernel::core::product::api::fill_owned_buffer(normalized, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_normalize_database_json(
    const char* json,
    const std::size_t json_size,
    kernel_owned_buffer* out_buffer) {
  if (out_buffer == nullptr || (json_size > 0 && json == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  const std::string_view raw(json == nullptr ? "" : json, json_size);
  std::string normalized;
  if (!kernel::core::product::normalize_database_json(raw, normalized)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  if (!kernel::core::product::api::fill_owned_buffer(normalized, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}
