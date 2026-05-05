// Reason: Expose context and path product rules through focused C ABI wrappers.

#include "kernel/c_api.h"

#include "core/kernel_product_context.h"
#include "core/kernel_product_public_api_utils.h"
#include "core/kernel_shared.h"

#include <string>
#include <string_view>

extern "C" kernel_status kernel_build_semantic_context(
    const char* content,
    const std::size_t content_size,
    kernel_owned_buffer* out_buffer) {
  if (out_buffer == nullptr || (content_size > 0 && content == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  const std::string_view raw(content == nullptr ? "" : content, content_size);
  const std::string context = kernel::core::product::build_semantic_context(raw);
  if (!kernel::core::product::api::fill_owned_buffer(context, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_get_semantic_context_min_bytes(std::size_t* out_bytes) {
  return kernel::core::product::api::write_size_limit(
      kernel::core::product::semantic_context_min_bytes(),
      out_bytes);
}

extern "C" kernel_status kernel_derive_file_extension_from_path(
    const char* path,
    const std::size_t path_size,
    kernel_owned_buffer* out_buffer) {
  if (out_buffer == nullptr || (path_size > 0 && path == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  const std::string_view path_view(path == nullptr ? "" : path, path_size);
  const std::string extension =
      kernel::core::product::derive_file_extension_from_path(path_view);
  if (!kernel::core::product::api::fill_owned_buffer(extension, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}
