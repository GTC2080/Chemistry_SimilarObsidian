// Reason: This file exposes symmetry operation search through the kernel C ABI
// so Tauri Rust no longer owns rotation/reflection matching rules.

#include "kernel/c_api.h"

#include "core/kernel_shared.h"
#include "symmetry/operation_search.h"

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

extern "C" kernel_status kernel_find_symmetry_rotation_axes(
    const kernel_symmetry_atom_input* atoms,
    const size_t atom_count,
    const kernel_symmetry_direction_input* candidates,
    const size_t candidate_count,
    kernel_symmetry_axis_input* out_axes,
    const size_t out_axis_capacity,
    size_t* out_axis_count) {
  if (out_axis_count != nullptr) {
    *out_axis_count = 0;
  }
  if (!has_valid_atom_inputs(atoms, atom_count) || out_axis_count == nullptr ||
      (candidate_count > 0 && candidates == nullptr) ||
      (out_axis_capacity > 0 && out_axes == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const auto result = kernel::symmetry::find_symmetry_rotation_axes(
      atoms,
      atom_count,
      candidates,
      candidate_count,
      out_axes,
      out_axis_capacity);
  *out_axis_count = result.count;
  if (result.capacity_exceeded) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_find_symmetry_mirror_planes(
    const kernel_symmetry_atom_input* atoms,
    const size_t atom_count,
    const kernel_symmetry_plane_input* candidates,
    const size_t candidate_count,
    kernel_symmetry_plane_input* out_planes,
    const size_t out_plane_capacity,
    size_t* out_plane_count) {
  if (out_plane_count != nullptr) {
    *out_plane_count = 0;
  }
  if (!has_valid_atom_inputs(atoms, atom_count) || out_plane_count == nullptr ||
      (candidate_count > 0 && candidates == nullptr) ||
      (out_plane_capacity > 0 && out_planes == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const auto result = kernel::symmetry::find_symmetry_mirror_planes(
      atoms,
      atom_count,
      candidates,
      candidate_count,
      out_planes,
      out_plane_capacity);
  *out_plane_count = result.count;
  if (result.capacity_exceeded) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  return kernel::core::make_status(KERNEL_OK);
}
