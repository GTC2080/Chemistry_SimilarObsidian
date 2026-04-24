// Reason: Keep point-group classification coverage at the kernel C ABI
// boundary so hosts can stay thin bridges over symmetry rules.

#include "kernel/c_api.h"

#include "api/kernel_api_test_suites.h"
#include "support/test_support.h"

#include <string>
#include <string_view>

namespace {

void require_ok_status(const kernel_status status, std::string_view context) {
  require_true(
      status.code == KERNEL_OK,
      std::string(context) + ": expected KERNEL_OK, got status " +
          std::to_string(status.code));
}

kernel_symmetry_axis_input axis(const double x, const double y, const double z, const uint8_t order) {
  kernel_symmetry_axis_input result{};
  result.dir[0] = x;
  result.dir[1] = y;
  result.dir[2] = z;
  result.order = order;
  return result;
}

kernel_symmetry_plane_input plane(const double x, const double y, const double z) {
  kernel_symmetry_plane_input result{};
  result.normal[0] = x;
  result.normal[1] = y;
  result.normal[2] = z;
  return result;
}

std::string classify(
    const kernel_symmetry_axis_input* axes,
    const size_t axis_count,
    const kernel_symmetry_plane_input* planes,
    const size_t plane_count,
    const bool has_inversion) {
  kernel_symmetry_classification_result result{};
  require_ok_status(
      kernel_classify_point_group(
          axes,
          axis_count,
          planes,
          plane_count,
          has_inversion ? 1 : 0,
          &result),
      "classify point group");
  return result.point_group;
}

void test_symmetry_classifies_c2v() {
  const kernel_symmetry_axis_input axes[] = {axis(0.0, 0.0, 1.0, 2)};
  const kernel_symmetry_plane_input planes[] = {
      plane(1.0, 0.0, 0.0),
      plane(0.0, 1.0, 0.0),
  };

  require_true(
      classify(axes, 1, planes, 2, false) == "C_2v",
      "symmetry classifier should classify C2v");
}

void test_symmetry_classifies_d2h() {
  const kernel_symmetry_axis_input axes[] = {
      axis(0.0, 0.0, 1.0, 2),
      axis(1.0, 0.0, 0.0, 2),
      axis(0.0, 1.0, 0.0, 2),
  };
  const kernel_symmetry_plane_input planes[] = {plane(0.0, 0.0, 1.0)};

  require_true(
      classify(axes, 3, planes, 1, true) == "D_2h",
      "symmetry classifier should classify D2h");
}

void test_symmetry_classifies_low_symmetry_cases() {
  const kernel_symmetry_plane_input mirror[] = {plane(1.0, 0.0, 0.0)};

  require_true(classify(nullptr, 0, nullptr, 0, false) == "C_1", "empty symmetry should be C1");
  require_true(classify(nullptr, 0, mirror, 1, false) == "C_s", "mirror only should be Cs");
  require_true(classify(nullptr, 0, nullptr, 0, true) == "C_i", "inversion only should be Ci");
}

void test_symmetry_rejects_invalid_inputs() {
  kernel_symmetry_classification_result result{};
  const auto valid_axis = axis(0.0, 0.0, 1.0, 2);
  const auto valid_plane = plane(1.0, 0.0, 0.0);

  require_true(
      kernel_classify_point_group(&valid_axis, 1, &valid_plane, 1, 0, nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "symmetry classifier should reject null output");
  require_true(
      kernel_classify_point_group(nullptr, 1, &valid_plane, 1, 0, &result).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "symmetry classifier should reject null axes when count is nonzero");
  require_true(
      kernel_classify_point_group(&valid_axis, 1, nullptr, 1, 0, &result).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "symmetry classifier should reject null planes when count is nonzero");
}

}  // namespace

void run_symmetry_compute_tests() {
  test_symmetry_classifies_c2v();
  test_symmetry_classifies_d2h();
  test_symmetry_classifies_low_symmetry_cases();
  test_symmetry_rejects_invalid_inputs();
}
