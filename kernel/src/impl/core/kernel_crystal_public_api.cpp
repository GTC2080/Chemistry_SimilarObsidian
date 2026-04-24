// Reason: This file exposes stateless crystal compute helpers through the
// kernel C ABI so Tauri Rust no longer owns Miller-plane math.

#include "kernel/c_api.h"

#include "core/kernel_shared.h"
#include "crystal/miller_plane.h"
#include "crystal/supercell.h"

#include <algorithm>
#include <cstring>
#include <new>
#include <vector>

namespace {

void reset_miller_plane_result(kernel_miller_plane_result* result) {
  if (result == nullptr) {
    return;
  }
  std::memset(result, 0, sizeof(kernel_miller_plane_result));
  result->error = KERNEL_CRYSTAL_MILLER_ERROR_NONE;
}

void reset_supercell_result(kernel_supercell_result* result) {
  if (result == nullptr) {
    return;
  }
  if (result->atoms != nullptr) {
    for (std::size_t index = 0; index < result->count; ++index) {
      delete[] result->atoms[index].element;
    }
    delete[] result->atoms;
  }
  result->atoms = nullptr;
  result->count = 0;
  result->estimated_count = 0;
  result->error = KERNEL_CRYSTAL_SUPERCELL_ERROR_NONE;
}

bool fill_supercell_result(
    const std::vector<kernel::crystal::AtomNodeView>& atoms,
    kernel_supercell_result* out_result) {
  if (atoms.empty()) {
    out_result->count = 0;
    out_result->atoms = nullptr;
    return true;
  }

  out_result->atoms = new (std::nothrow) kernel_atom_node[atoms.size()]{};
  if (out_result->atoms == nullptr) {
    return false;
  }
  out_result->count = atoms.size();

  for (std::size_t index = 0; index < atoms.size(); ++index) {
    const auto& source = atoms[index];
    auto& target = out_result->atoms[index];
    target.element = kernel::core::duplicate_c_string(source.element);
    if (target.element == nullptr) {
      return false;
    }
    for (std::size_t coord = 0; coord < 3; ++coord) {
      target.cartesian_coords[coord] = source.cartesian_coords[coord];
    }
  }
  return true;
}

}  // namespace

extern "C" kernel_status kernel_calculate_miller_plane(
    const kernel_crystal_cell_params* cell,
    const int32_t h,
    const int32_t k,
    const int32_t l,
    kernel_miller_plane_result* out_result) {
  reset_miller_plane_result(out_result);
  if (cell == nullptr || out_result == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const auto computation = kernel::crystal::calculate_miller_plane(*cell, h, k, l);
  if (computation.error != KERNEL_CRYSTAL_MILLER_ERROR_NONE) {
    out_result->error = computation.error;
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  *out_result = computation.result;
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_build_supercell(
    const kernel_crystal_cell_params* cell,
    const kernel_fractional_atom_input* atoms,
    const size_t atom_count,
    const kernel_symmetry_operation_input* symops,
    const size_t symop_count,
    const uint32_t nx,
    const uint32_t ny,
    const uint32_t nz,
    kernel_supercell_result* out_result) {
  reset_supercell_result(out_result);
  if (cell == nullptr || out_result == nullptr ||
      (atom_count > 0 && atoms == nullptr) || (symop_count > 0 && symops == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const auto computation =
      kernel::crystal::build_supercell(*cell, atoms, atom_count, symops, symop_count, nx, ny, nz);
  if (computation.error != KERNEL_CRYSTAL_SUPERCELL_ERROR_NONE) {
    out_result->error = computation.error;
    out_result->estimated_count = computation.estimated_count;
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  if (!fill_supercell_result(computation.atoms, out_result)) {
    reset_supercell_result(out_result);
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  out_result->error = KERNEL_CRYSTAL_SUPERCELL_ERROR_NONE;
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" void kernel_free_supercell_result(kernel_supercell_result* result) {
  reset_supercell_result(result);
}
