// Reason: Expose paper product rules through focused C ABI wrappers.

#include "kernel/c_api.h"

#include "core/kernel_product_paper.h"
#include "core/kernel_product_public_api_utils.h"
#include "core/kernel_shared.h"

#include <string>
#include <string_view>

extern "C" kernel_status kernel_build_paper_compile_plan_json(
    const char* workspace,
    const std::size_t workspace_size,
    const char* template_name,
    const std::size_t template_name_size,
    const char* const* image_paths,
    const std::size_t* image_path_sizes,
    const std::size_t image_path_count,
    const char* csl_path,
    const std::size_t csl_path_size,
    const char* bibliography_path,
    const std::size_t bibliography_path_size,
    const char* resource_separator,
    const std::size_t resource_separator_size,
    kernel_owned_buffer* out_buffer) {
  if (
      out_buffer == nullptr || (workspace_size > 0 && workspace == nullptr) ||
      (template_name_size > 0 && template_name == nullptr) ||
      (image_path_count > 0 && (image_paths == nullptr || image_path_sizes == nullptr)) ||
      (csl_path_size > 0 && csl_path == nullptr) ||
      (bibliography_path_size > 0 && bibliography_path == nullptr) ||
      (resource_separator_size > 0 && resource_separator == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  for (std::size_t index = 0; index < image_path_count; ++index) {
    if (image_path_sizes[index] > 0 && image_paths[index] == nullptr) {
      return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
    }
  }

  const std::string_view workspace_view(workspace == nullptr ? "" : workspace, workspace_size);
  const std::string_view template_view(
      template_name == nullptr ? "" : template_name,
      template_name_size);
  const std::string_view csl_view(csl_path == nullptr ? "" : csl_path, csl_path_size);
  const std::string_view bibliography_view(
      bibliography_path == nullptr ? "" : bibliography_path,
      bibliography_path_size);
  const std::string_view separator_view(
      resource_separator == nullptr ? "" : resource_separator,
      resource_separator_size);
  const std::string plan = kernel::core::product::build_paper_compile_plan_json(
      workspace_view,
      template_view,
      image_paths,
      image_path_sizes,
      image_path_count,
      csl_view,
      bibliography_view,
      separator_view);
  if (!kernel::core::product::api::fill_owned_buffer(plan, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_get_default_paper_template(kernel_owned_buffer* out_buffer) {
  if (out_buffer == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  if (!kernel::core::product::api::fill_owned_buffer(
          kernel::core::product::default_paper_template(),
          out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_summarize_paper_compile_log_json(
    const char* log,
    const std::size_t log_size,
    const std::size_t log_char_limit,
    kernel_owned_buffer* out_buffer) {
  if (out_buffer == nullptr || (log_size > 0 && log == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  const std::string_view raw(log == nullptr ? "" : log, log_size);
  const std::string summary =
      kernel::core::product::build_paper_compile_log_summary_json(raw, log_char_limit);
  if (!kernel::core::product::api::fill_owned_buffer(summary, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}
