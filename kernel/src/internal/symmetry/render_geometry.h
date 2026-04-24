// Reason: This file isolates symmetry viewer geometry construction in the
// kernel while hosts keep only DTO and ABI marshalling.

#pragma once

#include "kernel/types.h"

#include <cstddef>

namespace kernel::symmetry {

void build_symmetry_render_geometry(
    const kernel_symmetry_axis_input* axes,
    std::size_t axis_count,
    const kernel_symmetry_plane_input* planes,
    std::size_t plane_count,
    double mol_radius,
    kernel_symmetry_render_axis* out_axes,
    kernel_symmetry_render_plane* out_planes);

}  // namespace kernel::symmetry
