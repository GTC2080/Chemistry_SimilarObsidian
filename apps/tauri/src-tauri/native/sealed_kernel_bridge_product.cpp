#include "sealed_kernel_bridge_internal.h"

using namespace sealed_kernel_bridge_internal;

int32_t sealed_kernel_bridge_compute_truth_diff_json(
    const char* prev_content,
    uint64_t prev_size,
    const char* curr_content,
    uint64_t curr_size,
    const char* file_extension_utf8,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (out_json == nullptr ||
      file_extension_utf8 == nullptr ||
      (prev_size > 0 && prev_content == nullptr) ||
      (curr_size > 0 && curr_content == nullptr)) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_truth_diff_result result{};
  const kernel_status status = kernel_compute_truth_diff(
      prev_content,
      static_cast<size_t>(prev_size),
      curr_content,
      static_cast<size_t>(curr_size),
      file_extension_utf8,
      &result);
  if (status.code != KERNEL_OK) {
    SetError(
        out_error,
        status.code == KERNEL_ERROR_INVALID_ARGUMENT
            ? "invalid_argument"
            : "truth_diff_failed");
    kernel_free_truth_diff_result(&result);
    return static_cast<int32_t>(status.code);
  }

  std::string validation_error;
  if (!ValidateTruthDiffJsonInput(result, validation_error)) {
    SetError(out_error, validation_error);
    kernel_free_truth_diff_result(&result);
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }

  std::string json;
  AppendTruthDiffJson(json, result);
  kernel_free_truth_diff_result(&result);
  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "allocation_failed");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_build_semantic_context_text(
    const char* content,
    uint64_t content_size,
    char** out_text,
    char** out_error) {
  if (out_text != nullptr) {
    *out_text = nullptr;
  }
  if (out_text == nullptr || (content_size > 0 && content == nullptr)) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_owned_buffer buffer{};
  const kernel_status status = kernel_build_semantic_context(
      content,
      static_cast<size_t>(content_size),
      &buffer);
  if (status.code != KERNEL_OK) {
    SetError(
        out_error,
        status.code == KERNEL_ERROR_INVALID_ARGUMENT
            ? "invalid_argument"
            : "semantic_context_failed");
    kernel_free_buffer(&buffer);
    return static_cast<int32_t>(status.code);
  }

  const std::string text = buffer.data == nullptr || buffer.size == 0
                               ? std::string()
                               : std::string(buffer.data, buffer.size);
  kernel_free_buffer(&buffer);
  *out_text = CopyString(text);
  if (*out_text == nullptr) {
    SetError(out_error, "allocation_failed");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_derive_file_extension_from_path_text(
    const char* path_utf8,
    uint64_t path_size,
    char** out_text,
    char** out_error) {
  if (out_text != nullptr) {
    *out_text = nullptr;
  }
  if (out_text == nullptr || (path_size > 0 && path_utf8 == nullptr)) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_owned_buffer buffer{};
  const kernel_status status = kernel_derive_file_extension_from_path(
      path_utf8,
      static_cast<size_t>(path_size),
      &buffer);
  if (status.code != KERNEL_OK) {
    kernel_free_buffer(&buffer);
    return ReturnKernelError(status, "kernel_derive_file_extension_from_path", out_error);
  }

  return CopyKernelOwnedText(buffer, out_text, out_error);
}

int32_t sealed_kernel_bridge_derive_note_display_name_from_path_text(
    const char* path_utf8,
    uint64_t path_size,
    char** out_text,
    char** out_error) {
  if (out_text != nullptr) {
    *out_text = nullptr;
  }
  if (out_text == nullptr || (path_size > 0 && path_utf8 == nullptr)) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_owned_buffer buffer{};
  const kernel_status status = kernel_derive_note_display_name_from_path(
      path_utf8,
      static_cast<size_t>(path_size),
      &buffer);
  if (status.code != KERNEL_OK) {
    kernel_free_buffer(&buffer);
    return ReturnKernelError(status, "kernel_derive_note_display_name_from_path", out_error);
  }

  return CopyKernelOwnedText(buffer, out_text, out_error);
}

int32_t sealed_kernel_bridge_normalize_database_column_type_text(
    const char* column_type_utf8,
    uint64_t column_type_size,
    char** out_text,
    char** out_error) {
  if (out_text != nullptr) {
    *out_text = nullptr;
  }
  if (out_text == nullptr || (column_type_size > 0 && column_type_utf8 == nullptr)) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_owned_buffer buffer{};
  const kernel_status status = kernel_normalize_database_column_type(
      column_type_utf8,
      static_cast<size_t>(column_type_size),
      &buffer);
  if (status.code != KERNEL_OK) {
    kernel_free_buffer(&buffer);
    return ReturnKernelError(status, "kernel_normalize_database_column_type", out_error);
  }

  return CopyKernelOwnedText(buffer, out_text, out_error);
}

int32_t sealed_kernel_bridge_normalize_database_json(
    const char* json_utf8,
    uint64_t json_size,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (out_json == nullptr || (json_size > 0 && json_utf8 == nullptr)) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_owned_buffer buffer{};
  const kernel_status status = kernel_normalize_database_json(
      json_utf8,
      static_cast<size_t>(json_size),
      &buffer);
  if (status.code != KERNEL_OK) {
    kernel_free_buffer(&buffer);
    return ReturnKernelError(status, "kernel_normalize_database_json", out_error);
  }

  return CopyKernelOwnedText(buffer, out_json, out_error);
}

int32_t sealed_kernel_bridge_build_paper_compile_plan_json(
    const char* workspace_utf8,
    uint64_t workspace_size,
    const char* template_utf8,
    uint64_t template_size,
    const char* const* image_paths_utf8,
    const uint64_t* image_path_sizes,
    uint64_t image_path_count,
    const char* csl_path_utf8,
    uint64_t csl_path_size,
    const char* bibliography_path_utf8,
    uint64_t bibliography_path_size,
    const char* resource_separator_utf8,
    uint64_t resource_separator_size,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  const auto exceeds_size_t = [](uint64_t value) {
    return value > (std::numeric_limits<size_t>::max)();
  };
  if (
      out_json == nullptr || (workspace_size > 0 && workspace_utf8 == nullptr) ||
      (template_size > 0 && template_utf8 == nullptr) ||
      (image_path_count > 0 &&
       (image_paths_utf8 == nullptr || image_path_sizes == nullptr)) ||
      (csl_path_size > 0 && csl_path_utf8 == nullptr) ||
      (bibliography_path_size > 0 && bibliography_path_utf8 == nullptr) ||
      (resource_separator_size > 0 && resource_separator_utf8 == nullptr) ||
      exceeds_size_t(workspace_size) || exceeds_size_t(template_size) ||
      exceeds_size_t(image_path_count) || exceeds_size_t(csl_path_size) ||
      exceeds_size_t(bibliography_path_size) ||
      exceeds_size_t(resource_separator_size)) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::vector<size_t> image_sizes;
  image_sizes.reserve(static_cast<size_t>(image_path_count));
  for (uint64_t index = 0; index < image_path_count; ++index) {
    if (image_path_sizes[index] > (std::numeric_limits<size_t>::max)()) {
      SetError(out_error, "invalid_argument");
      return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
    }
    if (image_path_sizes[index] > 0 && image_paths_utf8[index] == nullptr) {
      SetError(out_error, "invalid_argument");
      return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
    }
    image_sizes.push_back(static_cast<size_t>(image_path_sizes[index]));
  }

  kernel_owned_buffer buffer{};
  const kernel_status status = kernel_build_paper_compile_plan_json(
      workspace_utf8,
      static_cast<size_t>(workspace_size),
      template_utf8,
      static_cast<size_t>(template_size),
      image_paths_utf8,
      image_sizes.data(),
      static_cast<size_t>(image_path_count),
      csl_path_utf8,
      static_cast<size_t>(csl_path_size),
      bibliography_path_utf8,
      static_cast<size_t>(bibliography_path_size),
      resource_separator_utf8,
      static_cast<size_t>(resource_separator_size),
      &buffer);
  if (status.code != KERNEL_OK) {
    kernel_free_buffer(&buffer);
    return ReturnKernelError(status, "kernel_build_paper_compile_plan_json", out_error);
  }

  return CopyKernelOwnedText(buffer, out_json, out_error);
}

int32_t sealed_kernel_bridge_get_default_paper_template(
    char** out_text,
    char** out_error) {
  if (out_text != nullptr) {
    *out_text = nullptr;
  }
  if (out_text == nullptr) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_owned_buffer buffer{};
  const kernel_status status = kernel_get_default_paper_template(&buffer);
  if (status.code != KERNEL_OK) {
    kernel_free_buffer(&buffer);
    return ReturnKernelError(status, "kernel_get_default_paper_template", out_error);
  }

  return CopyKernelOwnedText(buffer, out_text, out_error);
}

int32_t sealed_kernel_bridge_summarize_paper_compile_log_json(
    const char* log_utf8,
    uint64_t log_size,
    uint64_t log_char_limit,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  const auto exceeds_size_t = [](uint64_t value) {
    return value > (std::numeric_limits<size_t>::max)();
  };
  if (
      out_json == nullptr || (log_size > 0 && log_utf8 == nullptr) ||
      exceeds_size_t(log_size) || exceeds_size_t(log_char_limit)) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_owned_buffer buffer{};
  const kernel_status status = kernel_summarize_paper_compile_log_json(
      log_utf8,
      static_cast<size_t>(log_size),
      static_cast<size_t>(log_char_limit),
      &buffer);
  if (status.code != KERNEL_OK) {
    kernel_free_buffer(&buffer);
    return ReturnKernelError(status, "kernel_summarize_paper_compile_log_json", out_error);
  }

  return CopyKernelOwnedText(buffer, out_json, out_error);
}


