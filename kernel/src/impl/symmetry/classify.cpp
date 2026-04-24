// Reason: This file owns point-group classification rules formerly
// implemented in the Tauri Rust backend.

#include "symmetry/classify.h"

#include <cmath>
#include <cstdint>
#include <string>

namespace kernel::symmetry {
namespace {

std::size_t count_axes(
    const kernel_symmetry_axis_input* axes,
    const std::size_t axis_count,
    const std::uint8_t order) {
  std::size_t count = 0;
  for (std::size_t index = 0; index < axis_count; ++index) {
    if (axes[index].order == order) {
      ++count;
    }
  }
  return count;
}

double dot(const double a[3], const double b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

}  // namespace

std::string classify_point_group(
    const kernel_symmetry_axis_input* axes,
    const std::size_t axis_count,
    const kernel_symmetry_plane_input* planes,
    const std::size_t plane_count,
    const bool has_inversion) {
  const std::uint8_t highest_order = axis_count > 0 ? axes[0].order : 1;

  const std::size_t n_c3 = count_axes(axes, axis_count, 3);
  const std::size_t n_c4 = count_axes(axes, axis_count, 4);
  const std::size_t n_c5 = count_axes(axes, axis_count, 5);

  if (n_c5 >= 6) {
    return has_inversion ? "I_h" : "I";
  }

  if (n_c4 >= 3) {
    return has_inversion ? "O_h" : "O";
  }

  if (n_c3 >= 4 && highest_order <= 3) {
    if (plane_count >= 6) {
      return "T_d";
    }
    return has_inversion ? "T_h" : "T";
  }

  if (highest_order == 1) {
    if (plane_count > 0) {
      return "C_s";
    }
    if (has_inversion) {
      return "C_i";
    }
    return "C_1";
  }

  const std::uint8_t n = highest_order;
  const double* principal_dir = axes[0].dir;

  std::size_t perp_c2_count = 0;
  for (std::size_t index = 0; index < axis_count; ++index) {
    if (axes[index].order == 2 && std::abs(dot(axes[index].dir, principal_dir)) < 0.3) {
      ++perp_c2_count;
    }
  }

  bool has_sigma_h = false;
  std::size_t sigma_v_count = 0;
  for (std::size_t index = 0; index < plane_count; ++index) {
    const double projection = std::abs(dot(planes[index].normal, principal_dir));
    if (projection > 0.7) {
      has_sigma_h = true;
    }
    if (projection < 0.3) {
      ++sigma_v_count;
    }
  }

  if (perp_c2_count >= static_cast<std::size_t>(n)) {
    if (has_sigma_h) {
      return "D_" + std::to_string(n) + "h";
    }
    if (sigma_v_count >= static_cast<std::size_t>(n)) {
      return "D_" + std::to_string(n) + "d";
    }
    return "D_" + std::to_string(n);
  }

  if (has_sigma_h) {
    return "C_" + std::to_string(n) + "h";
  }
  if (sigma_v_count > 0) {
    return "C_" + std::to_string(n) + "v";
  }

  return "C_" + std::to_string(n);
}

}  // namespace kernel::symmetry
