// Reason: This file exposes symmetry molecule shape analysis through the
// kernel C ABI so Tauri Rust no longer owns center/linearity/inversion rules.

#include "kernel/c_api.h"

#include "core/kernel_shared.h"
#include "symmetry/shape_analysis.h"

#include <cstddef>
#include <cstring>

extern "C" kernel_status kernel_analyze_symmetry_shape(
    const kernel_symmetry_atom_input* atoms,
    const size_t atom_count,
    kernel_symmetry_shape_result* out_result) {
  if (out_result != nullptr) {
    std::memset(out_result, 0, sizeof(kernel_symmetry_shape_result));
  }
  if (out_result == nullptr || atoms == nullptr || atom_count == 0) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  for (std::size_t index = 0; index < atom_count; ++index) {
    if (atoms[index].element == nullptr) {
      return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
    }
  }

  *out_result = kernel::symmetry::analyze_symmetry_shape(atoms, atom_count);
  return kernel::core::make_status(KERNEL_OK);
}
