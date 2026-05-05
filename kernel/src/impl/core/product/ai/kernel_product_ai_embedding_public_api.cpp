// Reason: Expose AI embedding product rules through focused C ABI wrappers.

#include "kernel/c_api.h"

#include "core/kernel_product_ai.h"
#include "core/kernel_product_public_api_utils.h"
#include "core/kernel_shared.h"

#include <cstdint>
#include <string>
#include <string_view>

extern "C" kernel_status kernel_normalize_ai_embedding_text(
    const char* text,
    const std::size_t text_size,
    kernel_owned_buffer* out_buffer) {
  if (out_buffer == nullptr || (text_size > 0 && text == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  const std::string_view raw(text == nullptr ? "" : text, text_size);
  const std::string normalized = kernel::core::product::normalize_ai_embedding_text(raw);
  if (!kernel::core::product::is_ai_embedding_text_indexable(normalized)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  if (!kernel::core::product::api::fill_owned_buffer(normalized, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_is_ai_embedding_text_indexable(
    const char* text,
    const std::size_t text_size,
    std::uint8_t* out_is_indexable) {
  if (out_is_indexable == nullptr || (text_size > 0 && text == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string_view raw(text == nullptr ? "" : text, text_size);
  const std::string normalized = kernel::core::product::normalize_ai_embedding_text(raw);
  *out_is_indexable =
      static_cast<std::uint8_t>(kernel::core::product::is_ai_embedding_text_indexable(normalized));
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_should_refresh_ai_embedding_note(
    const std::int64_t note_updated_at,
    const std::uint8_t has_existing_updated_at,
    const std::int64_t existing_updated_at,
    std::uint8_t* out_should_refresh) {
  if (out_should_refresh == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  *out_should_refresh = static_cast<std::uint8_t>(
      has_existing_updated_at == 0 || note_updated_at > existing_updated_at);
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_compute_ai_embedding_cache_key(
    const char* base_url,
    const std::size_t base_url_size,
    const char* model,
    const std::size_t model_size,
    const char* text,
    const std::size_t text_size,
    kernel_owned_buffer* out_buffer) {
  if (
      out_buffer == nullptr || (base_url_size > 0 && base_url == nullptr) ||
      (model_size > 0 && model == nullptr) || (text_size > 0 && text == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  const std::string_view base_url_view(base_url == nullptr ? "" : base_url, base_url_size);
  const std::string_view model_view(model == nullptr ? "" : model, model_size);
  const std::string_view text_view(text == nullptr ? "" : text, text_size);
  const std::string key = kernel::core::product::compute_ai_embedding_cache_key(
      base_url_view,
      model_view,
      text_view);
  if (!kernel::core::product::api::fill_owned_buffer(key, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}
