// Reason: This file exposes end-to-end symmetry calculation through the kernel
// C ABI so Tauri Rust only maps command inputs and viewer DTO outputs.

#include "kernel/c_api.h"

#include "core/kernel_shared.h"
#include "symmetry/calculation.h"

#include <cstring>
#include <new>
#include <string_view>

namespace {

constexpr std::size_t kDefaultSymmetryAtomLimit = 500;

void reset_symmetry_calculation_result(kernel_symmetry_calculation_result* result) {
  if (result == nullptr) {
    return;
  }
  delete[] result->axes;
  delete[] result->planes;
  std::memset(result, 0, sizeof(kernel_symmetry_calculation_result));
}

bool fill_symmetry_calculation_result(
    const kernel::symmetry::SymmetryCalculation& source,
    kernel_symmetry_calculation_result* out_result) {
  out_result->atom_count = source.atom_count;
  out_result->has_inversion = source.has_inversion ? 1 : 0;
  out_result->error = source.error;
  out_result->parse_error = source.parse_error;

  if (source.point_group.size() >= KERNEL_SYMMETRY_POINT_GROUP_MAX) {
    return false;
  }
  std::memcpy(out_result->point_group, source.point_group.c_str(), source.point_group.size() + 1);

  if (!source.axes.empty()) {
    out_result->axes = new (std::nothrow) kernel_symmetry_render_axis[source.axes.size()]{};
    if (out_result->axes == nullptr) {
      return false;
    }
    out_result->axis_count = source.axes.size();
    std::memcpy(
        out_result->axes,
        source.axes.data(),
        sizeof(kernel_symmetry_render_axis) * source.axes.size());
  }

  if (!source.planes.empty()) {
    out_result->planes = new (std::nothrow) kernel_symmetry_render_plane[source.planes.size()]{};
    if (out_result->planes == nullptr) {
      return false;
    }
    out_result->plane_count = source.planes.size();
    std::memcpy(
        out_result->planes,
        source.planes.data(),
        sizeof(kernel_symmetry_render_plane) * source.planes.size());
  }

  return true;
}

}  // namespace

extern "C" kernel_status kernel_get_symmetry_atom_limit(std::size_t* out_atoms) {
  if (out_atoms == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  *out_atoms = kDefaultSymmetryAtomLimit;
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_calculate_symmetry(
    const char* raw,
    const size_t raw_size,
    const char* format,
    const size_t max_atoms,
    kernel_symmetry_calculation_result* out_result) {
  if (out_result == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  reset_symmetry_calculation_result(out_result);
  if (raw == nullptr || format == nullptr || max_atoms == 0) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const auto calculated = kernel::symmetry::calculate_symmetry(
      std::string_view(raw, raw_size),
      std::string_view(format),
      max_atoms);
  if (!fill_symmetry_calculation_result(calculated, out_result)) {
    reset_symmetry_calculation_result(out_result);
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  if (calculated.error != KERNEL_SYMMETRY_CALC_ERROR_NONE) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" void kernel_free_symmetry_calculation_result(
    kernel_symmetry_calculation_result* result) {
  reset_symmetry_calculation_result(result);
}
