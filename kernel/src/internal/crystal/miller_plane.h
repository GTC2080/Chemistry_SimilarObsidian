// Reason: This file isolates stateless crystal Miller-plane computation in
// the kernel while public full-result ABI keeps host crystal commands thin.

#pragma once

#include "kernel/types.h"

namespace kernel::crystal {

struct MillerPlaneComputation {
  kernel_crystal_miller_error error = KERNEL_CRYSTAL_MILLER_ERROR_NONE;
  kernel_miller_plane_result result{};
};

MillerPlaneComputation calculate_miller_plane(
    const kernel_crystal_cell_params& cell,
    int h,
    int k,
    int l);

}  // namespace kernel::crystal
