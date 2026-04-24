// Reason: This file isolates stateless point-group classification in the
// kernel while hosts keep molecule parsing and render DTO marshalling.

#pragma once

#include "kernel/types.h"

#include <cstddef>
#include <string>

namespace kernel::symmetry {

std::string classify_point_group(
    const kernel_symmetry_axis_input* axes,
    std::size_t axis_count,
    const kernel_symmetry_plane_input* planes,
    std::size_t plane_count,
    bool has_inversion);

}  // namespace kernel::symmetry
