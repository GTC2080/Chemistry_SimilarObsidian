// Reason: Expose AI embedding product rules through focused C ABI wrappers.

#include "kernel/c_api.h"

#include "core/kernel_product_ai.h"
#include "core/kernel_product_public_api_utils.h"
#include "core/kernel_shared.h"

#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <string>
#include <string_view>

namespace {

std::uint32_t float_to_u32(const float value) {
  std::uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

float u32_to_float(const std::uint32_t bits) {
  float value = 0.0F;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

void write_u32_le(const std::uint32_t value, char* out) {
  out[0] = static_cast<char>(value & 0xFFu);
  out[1] = static_cast<char>((value >> 8u) & 0xFFu);
  out[2] = static_cast<char>((value >> 16u) & 0xFFu);
  out[3] = static_cast<char>((value >> 24u) & 0xFFu);
}

std::uint32_t read_u32_le(const std::uint8_t* in) {
  return static_cast<std::uint32_t>(in[0]) |
         (static_cast<std::uint32_t>(in[1]) << 8u) |
         (static_cast<std::uint32_t>(in[2]) << 16u) |
         (static_cast<std::uint32_t>(in[3]) << 24u);
}

}  // namespace

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

extern "C" kernel_status kernel_serialize_ai_embedding_blob(
    const float* values,
    const std::size_t value_count,
    kernel_owned_buffer* out_buffer) {
  static_assert(sizeof(float) == 4, "AI embedding blob codec requires 32-bit floats");
  if (out_buffer == nullptr || (value_count > 0 && values == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  if (value_count > (std::numeric_limits<std::size_t>::max)() / 4u) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::string blob(value_count * 4u, '\0');
  for (std::size_t index = 0; index < value_count; ++index) {
    write_u32_le(float_to_u32(values[index]), blob.data() + (index * 4u));
  }
  if (!kernel::core::product::api::fill_owned_buffer(blob, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_parse_ai_embedding_blob(
    const std::uint8_t* blob,
    const std::size_t blob_size,
    kernel_float_buffer* out_values) {
  static_assert(sizeof(float) == 4, "AI embedding blob codec requires 32-bit floats");
  if (out_values == nullptr || (blob_size > 0 && blob == nullptr) || blob_size % 4u != 0u) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_values->values = nullptr;
  out_values->count = 0;

  const std::size_t value_count = blob_size / 4u;
  if (value_count == 0) {
    return kernel::core::make_status(KERNEL_OK);
  }

  auto* values = new (std::nothrow) float[value_count];
  if (values == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  for (std::size_t index = 0; index < value_count; ++index) {
    values[index] = u32_to_float(read_u32_le(blob + (index * 4u)));
  }

  out_values->values = values;
  out_values->count = value_count;
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" void kernel_free_float_buffer(kernel_float_buffer* buffer) {
  if (buffer == nullptr) {
    return;
  }
  delete[] buffer->values;
  buffer->values = nullptr;
  buffer->count = 0;
}
