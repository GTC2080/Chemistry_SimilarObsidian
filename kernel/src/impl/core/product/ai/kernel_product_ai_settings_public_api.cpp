// Reason: Expose AI product settings and path helpers through focused C ABI wrappers.

#include "kernel/c_api.h"

#include "core/kernel_product_ai.h"
#include "core/kernel_product_public_api_utils.h"
#include "core/kernel_shared.h"

#include <string>
#include <string_view>

extern "C" kernel_status kernel_get_rag_context_per_note_char_limit(std::size_t* out_chars) {
  return kernel::core::product::api::write_size_limit(
      kernel::core::product::rag_context_per_note_char_limit(),
      out_chars);
}

extern "C" kernel_status kernel_get_embedding_text_char_limit(std::size_t* out_chars) {
  return kernel::core::product::api::write_size_limit(
      kernel::core::product::embedding_text_char_limit(),
      out_chars);
}

extern "C" kernel_status kernel_get_ai_chat_timeout_secs(std::size_t* out_secs) {
  return kernel::core::product::api::write_size_limit(
      kernel::core::product::ai_chat_timeout_secs(),
      out_secs);
}

extern "C" kernel_status kernel_get_ai_ponder_timeout_secs(std::size_t* out_secs) {
  return kernel::core::product::api::write_size_limit(
      kernel::core::product::ai_ponder_timeout_secs(),
      out_secs);
}

extern "C" kernel_status kernel_get_ai_embedding_request_timeout_secs(std::size_t* out_secs) {
  return kernel::core::product::api::write_size_limit(
      kernel::core::product::ai_embedding_request_timeout_secs(),
      out_secs);
}

extern "C" kernel_status kernel_get_ai_embedding_cache_limit(std::size_t* out_limit) {
  return kernel::core::product::api::write_size_limit(
      kernel::core::product::ai_embedding_cache_limit(),
      out_limit);
}

extern "C" kernel_status kernel_get_ai_embedding_concurrency_limit(std::size_t* out_limit) {
  return kernel::core::product::api::write_size_limit(
      kernel::core::product::ai_embedding_concurrency_limit(),
      out_limit);
}

extern "C" kernel_status kernel_get_ai_rag_top_note_limit(std::size_t* out_limit) {
  return kernel::core::product::api::write_size_limit(
      kernel::core::product::ai_rag_top_note_limit(),
      out_limit);
}

extern "C" kernel_status kernel_derive_note_display_name_from_path(
    const char* path,
    const std::size_t path_size,
    kernel_owned_buffer* out_buffer) {
  if (out_buffer == nullptr || (path_size > 0 && path == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  const std::string_view path_view(path == nullptr ? "" : path, path_size);
  const std::string display_name(kernel::core::product::derive_note_display_name_from_path(path_view));
  if (!kernel::core::product::api::fill_owned_buffer(display_name, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}
