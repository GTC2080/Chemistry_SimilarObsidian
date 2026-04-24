// Reason: Keep stateless crystal compute coverage at the kernel C ABI boundary
// so hosts can stay thin bridges over Miller-plane math.

#include "kernel/c_api.h"

#include "api/kernel_api_test_suites.h"
#include "support/test_support.h"

#include <cmath>
#include <string>
#include <string_view>

namespace {

kernel_crystal_cell_params cubic_cell(const double a) {
  kernel_crystal_cell_params cell{};
  cell.a = a;
  cell.b = a;
  cell.c = a;
  cell.alpha_deg = 90.0;
  cell.beta_deg = 90.0;
  cell.gamma_deg = 90.0;
  return cell;
}

void require_ok_status(const kernel_status status, std::string_view context) {
  require_true(
      status.code == KERNEL_OK,
      std::string(context) + ": expected KERNEL_OK, got status " +
          std::to_string(status.code));
}

void require_near(const double actual, const double expected, std::string_view context) {
  require_true(
      std::abs(actual - expected) < 1.0e-9,
      std::string(context) + ": expected " + std::to_string(expected) + ", got " +
          std::to_string(actual));
}

void test_miller_plane_100_for_cubic_cell() {
  const auto cell = cubic_cell(5.0);
  kernel_miller_plane_result result{};

  require_ok_status(
      kernel_calculate_miller_plane(&cell, 1, 0, 0, &result),
      "miller plane 100");

  require_near(result.normal[0], 1.0, "miller 100 normal x");
  require_near(result.normal[1], 0.0, "miller 100 normal y");
  require_near(result.normal[2], 0.0, "miller 100 normal z");
  require_near(result.center[0], 5.0, "miller 100 center x");
  require_near(result.center[1], 0.0, "miller 100 center y");
  require_near(result.center[2], 0.0, "miller 100 center z");
  require_near(result.d, -5.0, "miller 100 plane d");
  require_true(result.error == KERNEL_CRYSTAL_MILLER_ERROR_NONE, "miller 100 should clear error");
}

void test_miller_plane_111_for_cubic_cell() {
  const auto cell = cubic_cell(5.0);
  kernel_miller_plane_result result{};

  require_ok_status(
      kernel_calculate_miller_plane(&cell, 1, 1, 1, &result),
      "miller plane 111");

  const double inv_sqrt3 = 1.0 / std::sqrt(3.0);
  require_near(result.normal[0], inv_sqrt3, "miller 111 normal x");
  require_near(result.normal[1], inv_sqrt3, "miller 111 normal y");
  require_near(result.normal[2], inv_sqrt3, "miller 111 normal z");
  require_true(result.vertices[0][0] != result.vertices[2][0], "miller 111 vertices should span");
}

void test_miller_plane_rejects_invalid_inputs_with_typed_errors() {
  auto cell = cubic_cell(5.0);
  kernel_miller_plane_result result{};

  require_true(
      kernel_calculate_miller_plane(&cell, 0, 0, 0, &result).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "miller should reject zero index");
  require_true(
      result.error == KERNEL_CRYSTAL_MILLER_ERROR_ZERO_INDEX,
      "miller should report zero index");

  cell.gamma_deg = 0.0;
  require_true(
      kernel_calculate_miller_plane(&cell, 1, 0, 0, &result).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "miller should reject degenerate gamma");
  require_true(
      result.error == KERNEL_CRYSTAL_MILLER_ERROR_GAMMA_TOO_SMALL,
      "miller should report degenerate gamma");

  require_true(
      kernel_calculate_miller_plane(nullptr, 1, 0, 0, &result).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "miller should reject null cell");
  require_true(
      kernel_calculate_miller_plane(&cell, 1, 0, 0, nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "miller should reject null output");
}

}  // namespace

void run_crystal_compute_tests() {
  test_miller_plane_100_for_cubic_cell();
  test_miller_plane_111_for_cubic_cell();
  test_miller_plane_rejects_invalid_inputs_with_typed_errors();
}
