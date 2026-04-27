// Reason: This file verifies the stateless PDF ink smoothing C ABI.

#include "kernel/c_api.h"

#include "api/kernel_api_pdf_surface_suites.h"
#include "support/test_support.h"

#include <cmath>

namespace {

kernel_ink_point point(const float x, const float y, const float pressure) {
  kernel_ink_point result{};
  result.x = x;
  result.y = y;
  result.pressure = pressure;
  return result;
}

void test_smooth_ink_strokes_interpolates_curved_stroke() {
  kernel_ink_point points[] = {
      point(0.0F, 0.0F, 0.5F),
      point(0.5F, 0.2F, 0.7F),
      point(1.0F, 0.0F, 0.9F),
  };
  kernel_ink_stroke_input stroke{};
  stroke.points = points;
  stroke.point_count = 3;
  stroke.stroke_width = 0.01F;

  kernel_ink_smoothing_result result{};
  expect_ok(kernel_smooth_ink_strokes(&stroke, 1, 0.001F, &result));
  require_true(result.count == 1, "ink smoothing should return one stroke");
  require_true(result.strokes != nullptr, "ink smoothing should allocate stroke output");
  require_true(result.strokes[0].point_count > 3, "curved stroke should be interpolated");
  require_true(
      std::abs(result.strokes[0].points[0].x - 0.0F) < 1.0e-6F,
      "ink smoothing should preserve first point");
  require_true(
      std::abs(result.strokes[0].points[result.strokes[0].point_count - 1].x - 1.0F) <
          1.0e-6F,
      "ink smoothing should preserve last point");
  require_true(result.strokes[0].stroke_width == 0.01F, "ink smoothing should preserve width");

  kernel_free_ink_smoothing_result(&result);
  require_true(
      result.strokes == nullptr && result.count == 0,
      "ink smoothing free should reset result");
  kernel_free_ink_smoothing_result(&result);
}

void test_pdf_ink_default_tolerance_is_kernel_owned() {
  float tolerance = 0.0F;
  expect_ok(kernel_get_pdf_ink_default_tolerance(&tolerance));
  require_true(
      std::abs(tolerance - 0.002F) < 1.0e-7F,
      "PDF ink default smoothing tolerance should come from kernel");
  require_true(
      kernel_get_pdf_ink_default_tolerance(nullptr).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "PDF ink default tolerance should reject null output");
}

void test_smooth_ink_strokes_preserves_two_point_strokes() {
  kernel_ink_point points[] = {
      point(0.0F, 0.0F, 0.5F),
      point(1.0F, 1.0F, 0.8F),
  };
  kernel_ink_stroke_input stroke{};
  stroke.points = points;
  stroke.point_count = 2;
  stroke.stroke_width = 0.02F;

  kernel_ink_smoothing_result result{};
  expect_ok(kernel_smooth_ink_strokes(&stroke, 1, 0.1F, &result));
  require_true(result.count == 1, "two-point smoothing should return one stroke");
  require_true(result.strokes[0].point_count == 2, "two-point stroke should stay two points");
  require_true(result.strokes[0].points[1].pressure == 0.8F, "pressure should be preserved");
  kernel_free_ink_smoothing_result(&result);
}

void test_smooth_ink_strokes_rejects_invalid_inputs() {
  kernel_ink_smoothing_result result{};
  kernel_ink_stroke_input stroke{};
  stroke.point_count = 1;
  stroke.points = nullptr;

  require_true(
      kernel_smooth_ink_strokes(nullptr, 1, 0.1F, &result).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "ink smoothing should reject null strokes with nonzero count");
  require_true(
      kernel_smooth_ink_strokes(&stroke, 1, 0.1F, &result).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "ink smoothing should reject null points with nonzero count");
  require_true(
      kernel_smooth_ink_strokes(nullptr, 0, 0.1F, nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "ink smoothing should reject null output");
}

}  // namespace

void run_pdf_surface_ink_tests() {
  test_pdf_ink_default_tolerance_is_kernel_owned();
  test_smooth_ink_strokes_interpolates_curved_stroke();
  test_smooth_ink_strokes_preserves_two_point_strokes();
  test_smooth_ink_strokes_rejects_invalid_inputs();
}
