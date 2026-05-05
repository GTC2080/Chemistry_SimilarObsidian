// Reason: Expose AI ponder product rules through focused C ABI wrappers.

#include "kernel/c_api.h"

#include "core/kernel_product_ai.h"
#include "core/kernel_product_public_api_utils.h"
#include "core/kernel_shared.h"

#include <string>
#include <string_view>

namespace {

kernel_status write_float_value(float value, float* out_value) {
  if (out_value == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  *out_value = value;
  return kernel::core::make_status(KERNEL_OK);
}

}  // namespace

extern "C" kernel_status kernel_get_ai_ponder_system_prompt(kernel_owned_buffer* out_buffer) {
  if (out_buffer == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  if (!kernel::core::product::api::fill_owned_buffer(
          kernel::core::product::ai_ponder_system_prompt(),
          out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_build_ai_ponder_user_prompt(
    const char* topic,
    const std::size_t topic_size,
    const char* context,
    const std::size_t context_size,
    kernel_owned_buffer* out_buffer) {
  if (
      out_buffer == nullptr || (topic_size > 0 && topic == nullptr) ||
      (context_size > 0 && context == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  const std::string_view topic_view(topic == nullptr ? "" : topic, topic_size);
  const std::string_view context_view(context == nullptr ? "" : context, context_size);
  const std::string prompt =
      kernel::core::product::build_ai_ponder_user_prompt(topic_view, context_view);
  if (!kernel::core::product::api::fill_owned_buffer(prompt, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_get_ai_ponder_temperature(float* out_temperature) {
  return write_float_value(kernel::core::product::ai_ponder_temperature(), out_temperature);
}
