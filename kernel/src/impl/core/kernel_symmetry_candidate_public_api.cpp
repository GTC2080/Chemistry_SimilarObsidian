// Reason: This file exposes symmetry candidate generation through the kernel
// C ABI so Tauri Rust no longer owns direction/plane candidate rules.

#include "kernel/c_api.h"

#include "core/kernel_shared.h"
#include "symmetry/candidate_generation.h"

#include <cstddef>

namespace {

bool has_valid_atom_inputs(const kernel_symmetry_atom_input* atoms, const std::size_t atom_count) {
  if (atoms == nullptr || atom_count == 0) {
    return false;
  }
  for (std::size_t index = 0; index < atom_count; ++index) {
    if (atoms[index].element == nullptr) {
      return false;
    }
  }
  return true;
}

}  // namespace

extern "C" kernel_status kernel_generate_symmetry_candidate_directions(
    const kernel_symmetry_atom_input* atoms,
    const size_t atom_count,
    const kernel_symmetry_direction_input* principal_axes,
    const size_t principal_axis_count,
    kernel_symmetry_direction_input* out_directions,
    const size_t out_direction_capacity,
    size_t* out_direction_count) {
  if (out_direction_count != nullptr) {
    *out_direction_count = 0;
  }
  if (!has_valid_atom_inputs(atoms, atom_count) || out_direction_count == nullptr ||
      (principal_axis_count > 0 && principal_axes == nullptr) ||
      (out_direction_capacity > 0 && out_directions == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const auto result = kernel::symmetry::generate_symmetry_candidate_directions(
      atoms,
      atom_count,
      principal_axes,
      principal_axis_count,
      out_directions,
      out_direction_capacity);
  *out_direction_count = result.count;
  if (result.capacity_exceeded) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_generate_symmetry_candidate_planes(
    const kernel_symmetry_atom_input* atoms,
    const size_t atom_count,
    const kernel_symmetry_axis_input* found_axes,
    const size_t axis_count,
    const kernel_symmetry_direction_input* principal_axes,
    const size_t principal_axis_count,
    kernel_symmetry_plane_input* out_planes,
    const size_t out_plane_capacity,
    size_t* out_plane_count) {
  if (out_plane_count != nullptr) {
    *out_plane_count = 0;
  }
  if (!has_valid_atom_inputs(atoms, atom_count) || out_plane_count == nullptr ||
      (axis_count > 0 && found_axes == nullptr) ||
      (principal_axis_count > 0 && principal_axes == nullptr) ||
      (out_plane_capacity > 0 && out_planes == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const auto result = kernel::symmetry::generate_symmetry_candidate_planes(
      atoms,
      atom_count,
      found_axes,
      axis_count,
      principal_axes,
      principal_axis_count,
      out_planes,
      out_plane_capacity);
  *out_plane_count = result.count;
  if (result.capacity_exceeded) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  return kernel::core::make_status(KERNEL_OK);
}
