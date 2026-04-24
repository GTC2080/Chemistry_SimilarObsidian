// Reason: Keep chemistry stoichiometry coverage at the kernel ABI boundary so
// hosts can stay thin bridges over the shared numeric rules.

#include "kernel/c_api.h"

#include "api/kernel_api_chemistry_suites.h"
#include "support/test_support.h"

#include <cmath>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace {

kernel_stoichiometry_row_input row(
    const double mw,
    const double eq,
    const double moles,
    const double mass,
    const double volume,
    const double density,
    const bool has_density,
    const bool is_reference) {
  kernel_stoichiometry_row_input result{};
  result.mw = mw;
  result.eq = eq;
  result.moles = moles;
  result.mass = mass;
  result.volume = volume;
  result.density = density;
  result.has_density = has_density ? 1 : 0;
  result.is_reference = is_reference ? 1 : 0;
  return result;
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

void test_stoichiometry_uses_first_marked_reference_row() {
  const std::vector<kernel_stoichiometry_row_input> input{
      row(50.0, 2.0, 99.0, 0.0, 0.0, 0.0, false, false),
      row(100.0, 7.0, 0.25, 9.0, 3.0, 2.0, true, true),
      row(10.0, 3.0, 99.0, 8.0, 4.0, 0.0, false, true),
  };
  std::vector<kernel_stoichiometry_row_output> output(input.size());

  require_ok_status(
      kernel_recalculate_stoichiometry(input.data(), input.size(), output.data()),
      "stoichiometry marked reference");

  require_true(output[0].is_reference == 0, "first row should not be reference");
  require_true(output[1].is_reference == 1, "first marked row should be reference");
  require_true(output[2].is_reference == 0, "later marked rows should be normalized away");

  require_near(output[1].eq, 1.0, "reference eq");
  require_near(output[1].moles, 0.25, "reference moles");
  require_near(output[1].mass, 25.0, "reference mass");
  require_true(output[1].has_density == 1, "reference should preserve explicit density");
  require_near(output[1].density, 2.0, "reference density");
  require_near(output[1].volume, 12.5, "reference volume");

  require_near(output[2].eq, 3.0, "dependent eq");
  require_near(output[2].moles, 0.75, "dependent moles");
  require_near(output[2].mass, 7.5, "dependent mass");
  require_true(output[2].has_density == 1, "dependent should infer density");
  require_near(output[2].density, 2.0, "dependent inferred density");
  require_near(output[2].volume, 3.75, "dependent inferred volume");
}

void test_stoichiometry_defaults_first_row_as_reference() {
  const std::vector<kernel_stoichiometry_row_input> input{
      row(20.0, 4.0, 0.5, 0.0, 0.0, 5.0, true, false),
      row(30.0, 2.0, 7.0, 0.0, 0.0, 0.0, false, false),
  };
  std::vector<kernel_stoichiometry_row_output> output(input.size());

  require_ok_status(
      kernel_recalculate_stoichiometry(input.data(), input.size(), output.data()),
      "stoichiometry default reference");

  require_true(output[0].is_reference == 1, "first row should become default reference");
  require_near(output[0].eq, 1.0, "default reference eq");
  require_near(output[0].moles, 0.5, "default reference moles");
  require_near(output[0].mass, 10.0, "default reference mass");
  require_near(output[0].volume, 2.0, "default reference volume");

  require_true(output[1].is_reference == 0, "second row should not be reference");
  require_near(output[1].moles, 1.0, "second row moles");
  require_near(output[1].mass, 30.0, "second row mass");
}

void test_stoichiometry_clamps_invalid_numeric_inputs() {
  const std::vector<kernel_stoichiometry_row_input> input{
      row(
          std::numeric_limits<double>::quiet_NaN(),
          4.0,
          -1.0,
          0.0,
          0.0,
          0.0,
          false,
          true),
      row(
          std::numeric_limits<double>::infinity(),
          -3.0,
          7.0,
          1.0,
          0.0,
          0.0,
          false,
          false),
  };
  std::vector<kernel_stoichiometry_row_output> output(input.size());

  require_ok_status(
      kernel_recalculate_stoichiometry(input.data(), input.size(), output.data()),
      "stoichiometry invalid numeric clamp");

  require_near(output[0].mw, 0.0, "invalid reference mw");
  require_near(output[0].moles, 0.0, "invalid reference moles");
  require_near(output[0].mass, 0.0, "invalid reference mass");
  require_near(output[1].eq, 0.0, "invalid dependent eq");
  require_near(output[1].mw, 0.0, "invalid dependent mw");
  require_near(output[1].moles, 0.0, "invalid dependent moles");
  require_near(output[1].mass, 0.0, "invalid dependent mass");
  require_true(output[1].has_density == 0, "invalid dependent density should be absent");
}

void test_stoichiometry_validates_null_arguments() {
  kernel_stoichiometry_row_input input{};
  kernel_stoichiometry_row_output output{};

  require_true(
      kernel_recalculate_stoichiometry(nullptr, 1, &output).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "stoichiometry should reject null rows");
  require_true(
      kernel_recalculate_stoichiometry(&input, 1, nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "stoichiometry should reject null output");
  require_true(
      kernel_recalculate_stoichiometry(nullptr, 0, nullptr).code == KERNEL_OK,
      "stoichiometry should accept zero-count null buffers");
}

}  // namespace

void run_chemistry_stoichiometry_tests() {
  test_stoichiometry_uses_first_marked_reference_row();
  test_stoichiometry_defaults_first_row_as_reference();
  test_stoichiometry_clamps_invalid_numeric_inputs();
  test_stoichiometry_validates_null_arguments();
}
