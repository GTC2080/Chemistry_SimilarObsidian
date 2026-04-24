// Reason: This file isolates stateless supercell expansion in the kernel while
// hosts keep CIF parsing and command DTO marshalling.

#pragma once

#include "kernel/types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace kernel::crystal {

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
