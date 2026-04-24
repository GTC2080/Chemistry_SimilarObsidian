// Reason: This file isolates stateless symmetry molecule shape analysis in
// the kernel while hosts keep only molecule DTO marshalling.

#pragma once

#include "kernel/types.h"

#include <cstddef>

namespace kernel::symmetry {

kernel_symmetry_shape_result analyze_symmetry_shape(
    const kernel_symmetry_atom_input* atoms,
    std::size_t atom_count);

}  // namespace kernel::symmetry
