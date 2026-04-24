// Reason: This file isolates symmetry operation validation in the kernel
// while hosts keep only candidate generation and ABI marshalling.

#pragma once

#include "kernel/types.h"

#include <cstddef>

namespace kernel::symmetry {

struct RotationAxisSearchResult {
  std::size_t count = 0;
  bool capacity_exceeded = false;
};

struct MirrorPlaneSearchResult {
  std::size_t count = 0;
  bool capacity_exceeded = false;
};

RotationAxisSearchResult find_symmetry_rotation_axes(
    const kernel_symmetry_atom_input* atoms,
    std::size_t atom_count,
    const kernel_symmetry_direction_input* candidates,
    std::size_t candidate_count,
    kernel_symmetry_axis_input* out_axes,
    std::size_t out_axis_capacity);

MirrorPlaneSearchResult find_symmetry_mirror_planes(
    const kernel_symmetry_atom_input* atoms,
    std::size_t atom_count,
    const kernel_symmetry_plane_input* candidates,
    std::size_t candidate_count,
    kernel_symmetry_plane_input* out_planes,
    std::size_t out_plane_capacity);

}  // namespace kernel::symmetry
