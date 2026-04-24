// Reason: This file isolates stateless symmetry principal-axis calculation in
// the kernel while hosts keep only molecule DTO marshalling.

#pragma once

#include "kernel/types.h"

#include <cstddef>

namespace kernel::symmetry {

void compute_symmetry_principal_axes(
    const kernel_symmetry_atom_input* atoms,
    std::size_t atom_count,
    kernel_symmetry_direction_input out_axes[3]);

}  // namespace kernel::symmetry
