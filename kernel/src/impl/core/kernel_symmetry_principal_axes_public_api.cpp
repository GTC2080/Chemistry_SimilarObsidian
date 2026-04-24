// Reason: This file exposes symmetry principal-axis calculation through the
// kernel C ABI so Tauri Rust no longer owns inertia tensor rules.

#include "kernel/c_api.h"

#include "core/kernel_shared.h"
#include "symmetry/principal_axes.h"

#include <cstddef>
#include <cstring>

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

extern "C" kernel_status kernel_compute_symmetry_principal_axes(
    const kernel_symmetry_atom_input* atoms,
    const size_t atom_count,
    kernel_symmetry_direction_input* out_axes) {
  if (out_axes != nullptr) {
    std::memset(out_axes, 0, sizeof(kernel_symmetry_direction_input) * 3);
  }
  if (!has_valid_atom_inputs(atoms, atom_count) || out_axes == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel::symmetry::compute_symmetry_principal_axes(atoms, atom_count, out_axes);
  return kernel::core::make_status(KERNEL_OK);
}
