// Reason: Keep polymerization kinetics coverage beside chemistry API suites so
// the Tauri command can remain a bridge over the C++ compute kernel.

#include "kernel/c_api.h"

#include "api/kernel_api_chemistry_suites.h"
#include "support/test_support.h"

#include <cmath>
#include <string>
#include <string_view>

namespace {

kernel_polymerization_kinetics_params default_params() {
  kernel_polymerization_kinetics_params params{};
  params.m0 = 1.0;
  params.i0 = 0.01;
  params.cta0 = 0.001;
  params.kd = 0.001;
  params.kp = 100.0;
  params.kt = 1000.0;
  params.ktr = 0.1;
  params.time_max = 3600.0;
  params.steps = 120;
  return params;
}

void require_ok_status(const kernel_status status, std::string_view context) {
  require_true(
      status.code == KERNEL_OK,
      std::string(context) + ": expected KERNEL_OK, got status " +
          std::to_string(status.code));
}

void require_finite_series(
    const kernel_polymerization_kinetics_result& result,
    std::string_view context) {
  for (std::size_t index = 0; index < result.count; ++index) {
    require_true(std::isfinite(result.time[index]), std::string(context) + ": finite time");
    require_true(
        std::isfinite(result.conversion[index]),
        std::string(context) + ": finite conversion");
    require_true(std::isfinite(result.mn[index]), std::string(context) + ": finite mn");
    require_true(std::isfinite(result.pdi[index]), std::string(context) + ": finite pdi");
  }
}

void test_polymerization_kinetics_returns_stable_series() {
  const auto params = default_params();
  kernel_polymerization_kinetics_result result{};
  require_ok_status(
      kernel_simulate_polymerization_kinetics(&params, &result),
      "polymerization kinetics simulation");

  require_true(result.count == params.steps + 1, "kinetics should emit steps + 1 samples");
  require_true(result.time != nullptr, "kinetics should allocate time");
  require_true(result.conversion != nullptr, "kinetics should allocate conversion");
  require_true(result.mn != nullptr, "kinetics should allocate mn");
  require_true(result.pdi != nullptr, "kinetics should allocate pdi");
  require_true(result.time[0] == 0.0, "kinetics should start at t=0");
  require_true(
      std::abs(result.time[result.count - 1] - params.time_max) < 1.0e-9,
      "kinetics should end at time_max");
  require_true(result.conversion[0] == 0.0, "kinetics should start with zero conversion");
  require_true(
      result.conversion[result.count - 1] > result.conversion[0],
      "kinetics should increase conversion over time");
  require_true(result.pdi[0] >= 1.0, "kinetics should clamp initial pdi");
  require_true(
      result.pdi[result.count - 1] >= 1.0,
      "kinetics should keep pdi physically bounded");
  require_finite_series(result, "polymerization kinetics finite output");

  kernel_free_polymerization_kinetics_result(&result);
  require_true(
      result.time == nullptr && result.conversion == nullptr &&
          result.mn == nullptr && result.pdi == nullptr && result.count == 0,
      "kinetics free should reset result");
}

void test_polymerization_kinetics_rejects_invalid_params_and_clears_output() {
  auto params = default_params();
  kernel_polymerization_kinetics_result result{};
  require_ok_status(
      kernel_simulate_polymerization_kinetics(&params, &result),
      "polymerization kinetics seed result");
  require_true(result.count > 0, "kinetics seed result should allocate output");

  params.m0 = -1.0;
  const kernel_status invalid_status =
      kernel_simulate_polymerization_kinetics(&params, &result);
  require_true(
      invalid_status.code == KERNEL_ERROR_INVALID_ARGUMENT,
      "kinetics should reject invalid physical parameters");
  require_true(
      result.time == nullptr && result.conversion == nullptr &&
          result.mn == nullptr && result.pdi == nullptr && result.count == 0,
      "kinetics invalid call should clear stale output");

  params = default_params();
  params.steps = 9;
  require_true(
      kernel_simulate_polymerization_kinetics(&params, &result).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "kinetics should reject too few steps");
  params.steps = 50001;
  require_true(
      kernel_simulate_polymerization_kinetics(&params, &result).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "kinetics should reject too many steps");
  require_true(
      kernel_simulate_polymerization_kinetics(nullptr, &result).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "kinetics should reject null params");
  require_true(
      kernel_simulate_polymerization_kinetics(&params, nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "kinetics should reject null output");
}

}  // namespace

void run_chemistry_kinetics_tests() {
  test_polymerization_kinetics_returns_stable_series();
  test_polymerization_kinetics_rejects_invalid_params_and_clears_output();
}
