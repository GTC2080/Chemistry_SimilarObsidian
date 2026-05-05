// Reason: Expose AI RAG product rules through focused C ABI wrappers.

#include "kernel/c_api.h"

#include "core/kernel_product_ai.h"
#include "core/kernel_product_public_api_utils.h"
#include "core/kernel_shared.h"

#include <string>
#include <string_view>

extern "C" kernel_status kernel_build_ai_rag_context(
    const char* const* note_names,
    const std::size_t* note_name_sizes,
    const char* const* note_contents,
    const std::size_t* note_content_sizes,
    const std::size_t note_count,
    kernel_owned_buffer* out_buffer) {
  if (
      out_buffer == nullptr ||
      (note_count > 0 &&
       (note_names == nullptr || note_name_sizes == nullptr || note_contents == nullptr ||
        note_content_sizes == nullptr))) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  for (std::size_t index = 0; index < note_count; ++index) {
    if (
        (note_name_sizes[index] > 0 && note_names[index] == nullptr) ||
        (note_content_sizes[index] > 0 && note_contents[index] == nullptr)) {
      return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
    }
  }

  const std::string context = kernel::core::product::build_ai_rag_context(
      note_names,
      note_name_sizes,
      note_contents,
      note_content_sizes,
      note_count);
  if (!kernel::core::product::api::fill_owned_buffer(context, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_build_ai_rag_context_from_note_paths(
    const char* const* note_paths,
    const std::size_t* note_path_sizes,
    const char* const* note_contents,
    const std::size_t* note_content_sizes,
    const std::size_t note_count,
    kernel_owned_buffer* out_buffer) {
  if (
      out_buffer == nullptr ||
      (note_count > 0 &&
       (note_paths == nullptr || note_path_sizes == nullptr || note_contents == nullptr ||
        note_content_sizes == nullptr))) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  for (std::size_t index = 0; index < note_count; ++index) {
    if (
        (note_path_sizes[index] > 0 && note_paths[index] == nullptr) ||
        (note_content_sizes[index] > 0 && note_contents[index] == nullptr)) {
      return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
    }
  }

  const std::string context = kernel::core::product::build_ai_rag_context_from_note_paths(
      note_paths,
      note_path_sizes,
      note_contents,
      note_content_sizes,
      note_count);
  if (!kernel::core::product::api::fill_owned_buffer(context, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_build_ai_rag_system_content(
    const char* context,
    const std::size_t context_size,
    kernel_owned_buffer* out_buffer) {
  if (out_buffer == nullptr || (context_size > 0 && context == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  const std::string_view context_view(context == nullptr ? "" : context, context_size);
  const std::string content = kernel::core::product::build_ai_rag_system_content(context_view);
  if (!kernel::core::product::api::fill_owned_buffer(content, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}
