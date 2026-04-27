// Reason: This file isolates stateless supercell expansion in the kernel while
// public full-result ABI keeps host crystal commands thin.

#pragma once

#include "kernel/types.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace kernel::crystal {

inline constexpr std::size_t kMaxSupercellAtoms = 50000;

struct AtomNodeView {
  std::string element;
  double cartesian_coords[3]{};
};

struct SupercellComputation {
  kernel_crystal_supercell_error error = KERNEL_CRYSTAL_SUPERCELL_ERROR_NONE;
  std::uint64_t estimated_count = 0;
  std::vector<AtomNodeView> atoms;
};

SupercellComputation build_supercell(
    const kernel_crystal_cell_params& cell,
    const kernel_fractional_atom_input* atoms,
    std::size_t atom_count,
    const kernel_symmetry_operation_input* symops,
    std::size_t symop_count,
    std::uint32_t nx,
    std::uint32_t ny,
    std::uint32_t nz);

}  // namespace kernel::crystal
