#include "sealed_kernel_bridge_internal.h"

using namespace sealed_kernel_bridge_internal;

int32_t sealed_kernel_bridge_get_pdf_ink_default_tolerance(
    float* out_tolerance,
    char** out_error) {
  if (out_tolerance == nullptr) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const kernel_status status = kernel_get_pdf_ink_default_tolerance(out_tolerance);
  if (status.code != KERNEL_OK) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(status.code);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_compute_pdf_file_lightweight_hash(
    sealed_kernel_bridge_session* session,
    const char* host_path_utf8,
    uint64_t host_path_size,
    char** out_hash,
    char** out_error) {
  if (out_hash != nullptr) {
    *out_hash = nullptr;
  }
  if (session == nullptr || out_hash == nullptr || (host_path_size > 0 && host_path_utf8 == nullptr)) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string host_path_utf8_value =
      host_path_utf8 == nullptr
          ? std::string()
          : std::string(host_path_utf8, static_cast<std::size_t>(host_path_size));
  const std::string host_path = Utf8ToActiveCodePage(host_path_utf8_value.c_str());

  kernel_owned_buffer buffer{};
  const kernel_status status = kernel_compute_pdf_file_lightweight_hash(
      session->handle,
      host_path.c_str(),
      host_path.size(),
      &buffer);
  if (status.code != KERNEL_OK) {
    kernel_free_buffer(&buffer);
    return ReturnKernelError(status, "kernel_compute_pdf_file_lightweight_hash", out_error);
  }

  const std::string hash =
      buffer.data == nullptr ? std::string() : std::string(buffer.data, buffer.size);
  kernel_free_buffer(&buffer);
  *out_hash = CopyString(hash);
  if (*out_hash == nullptr) {
    SetError(out_error, "allocation_failed");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_read_pdf_annotation_json(
    sealed_kernel_bridge_session* session,
    const char* pdf_rel_path_utf8,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (session == nullptr || out_json == nullptr || pdf_rel_path_utf8 == nullptr || pdf_rel_path_utf8[0] == '\0') {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string pdf_rel_path = Utf8ToActiveCodePage(pdf_rel_path_utf8);
  kernel_owned_buffer buffer{};
  const kernel_status status =
      kernel_read_pdf_annotation_file(session->handle, pdf_rel_path.c_str(), &buffer);
  if (status.code != KERNEL_OK) {
    kernel_free_buffer(&buffer);
    return ReturnKernelError(status, "kernel_read_pdf_annotation_file", out_error);
  }

  const std::string json =
      buffer.data == nullptr ? std::string() : std::string(buffer.data, buffer.size);
  kernel_free_buffer(&buffer);
  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "allocation_failed");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_write_pdf_annotation_json(
    sealed_kernel_bridge_session* session,
    const char* pdf_rel_path_utf8,
    const char* json_utf8,
    uint64_t json_size,
    char** out_error) {
  if (
      session == nullptr || pdf_rel_path_utf8 == nullptr || pdf_rel_path_utf8[0] == '\0' ||
      (json_size > 0 && json_utf8 == nullptr)) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string pdf_rel_path = Utf8ToActiveCodePage(pdf_rel_path_utf8);
  const kernel_status status = kernel_write_pdf_annotation_file(
      session->handle,
      pdf_rel_path.c_str(),
      json_utf8,
      static_cast<std::size_t>(json_size));
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_write_pdf_annotation_file", out_error);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_smooth_ink_strokes_json(
    const float* xs,
    const float* ys,
    const float* pressures,
    const uint64_t* point_counts,
    const float* stroke_widths,
    uint64_t stroke_count,
    float tolerance,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (out_json == nullptr) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  if (stroke_count > 0 && (point_counts == nullptr || stroke_widths == nullptr)) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  size_t total_point_count = 0;
  std::vector<size_t> counts;
  counts.reserve(static_cast<size_t>(stroke_count));
  for (uint64_t stroke_index = 0; stroke_index < stroke_count; ++stroke_index) {
    const size_t count = static_cast<size_t>(point_counts[stroke_index]);
    counts.push_back(count);
    total_point_count += count;
  }
  if (total_point_count > 0 && (xs == nullptr || ys == nullptr || pressures == nullptr)) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::vector<kernel_ink_point> points;
  points.reserve(total_point_count);
  for (size_t point_index = 0; point_index < total_point_count; ++point_index) {
    points.push_back(kernel_ink_point{xs[point_index], ys[point_index], pressures[point_index]});
  }

  std::vector<kernel_ink_stroke_input> strokes;
  strokes.reserve(static_cast<size_t>(stroke_count));
  size_t point_offset = 0;
  for (uint64_t stroke_index = 0; stroke_index < stroke_count; ++stroke_index) {
    const size_t count = counts[static_cast<size_t>(stroke_index)];
    kernel_ink_stroke_input stroke{};
    stroke.points = count == 0 ? nullptr : points.data() + point_offset;
    stroke.point_count = count;
    stroke.stroke_width = stroke_widths[stroke_index];
    strokes.push_back(stroke);
    point_offset += count;
  }

  kernel_ink_smoothing_result result{};
  const kernel_status status = kernel_smooth_ink_strokes(
      strokes.empty() ? nullptr : strokes.data(),
      strokes.size(),
      tolerance,
      &result);
  if (status.code != KERNEL_OK) {
    SetError(
        out_error,
        status.code == KERNEL_ERROR_INVALID_ARGUMENT ? "invalid_argument" : "ink_smoothing_failed");
    kernel_free_ink_smoothing_result(&result);
    return static_cast<int32_t>(status.code);
  }

  std::string validation_error;
  if (!ValidateInkSmoothingJsonInput(result, validation_error)) {
    SetError(out_error, validation_error);
    kernel_free_ink_smoothing_result(&result);
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }

  std::string json;
  AppendInkSmoothingJson(json, result);
  kernel_free_ink_smoothing_result(&result);
  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "allocation_failed");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}


