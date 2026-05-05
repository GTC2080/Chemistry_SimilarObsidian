// Reason: This file exposes PDF ink smoothing through the kernel C ABI so
// Tauri Rust no longer owns the stroke simplification/smoothing algorithm.

#include "kernel/c_api.h"

#include "core/kernel_shared.h"
#include "pdf/ink_smoothing.h"

#include <algorithm>
#include <new>
#include <vector>

namespace {

constexpr float kDefaultInkSmoothingTolerance = 0.002F;

void reset_ink_smoothing_result(kernel_ink_smoothing_result* result) {
  if (result == nullptr) {
    return;
  }
  if (result->strokes != nullptr) {
    for (std::size_t index = 0; index < result->count; ++index) {
      delete[] result->strokes[index].points;
      result->strokes[index].points = nullptr;
      result->strokes[index].point_count = 0;
      result->strokes[index].stroke_width = 0.0F;
    }
    delete[] result->strokes;
  }
  result->strokes = nullptr;
  result->count = 0;
}

bool fill_ink_smoothing_result(
    const std::vector<kernel::pdf::SmoothedInkStroke>& strokes,
    kernel_ink_smoothing_result* out_result) {
  if (strokes.empty()) {
    return true;
  }

  out_result->strokes = new (std::nothrow) kernel_ink_stroke[strokes.size()]{};
  if (out_result->strokes == nullptr) {
    return false;
  }
  out_result->count = strokes.size();

  for (std::size_t index = 0; index < strokes.size(); ++index) {
    const auto& source = strokes[index];
    auto& target = out_result->strokes[index];
    target.stroke_width = source.stroke_width;
    target.point_count = source.points.size();
    if (source.points.empty()) {
      continue;
    }
    target.points = new (std::nothrow) kernel_ink_point[source.points.size()];
    if (target.points == nullptr) {
      return false;
    }
    std::copy(source.points.begin(), source.points.end(), target.points);
  }
  return true;
}

}  // namespace

extern "C" kernel_status kernel_get_pdf_ink_default_tolerance(float* out_tolerance) {
  if (out_tolerance == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  *out_tolerance = kDefaultInkSmoothingTolerance;
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_smooth_ink_strokes(
    const kernel_ink_stroke_input* strokes,
    const size_t stroke_count,
    const float tolerance,
    kernel_ink_smoothing_result* out_result) {
  reset_ink_smoothing_result(out_result);
  if (out_result == nullptr || (stroke_count > 0 && strokes == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  for (std::size_t index = 0; index < stroke_count; ++index) {
    if (strokes[index].point_count > 0 && strokes[index].points == nullptr) {
      return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
    }
  }

  const auto smoothed = kernel::pdf::smooth_ink_strokes(strokes, stroke_count, tolerance);
  if (!fill_ink_smoothing_result(smoothed, out_result)) {
    reset_ink_smoothing_result(out_result);
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" void kernel_free_ink_smoothing_result(kernel_ink_smoothing_result* result) {
  reset_ink_smoothing_result(result);
}
