// Reason: This file isolates symmetry candidate generation in the kernel
// while hosts keep only principal-axis calculation and ABI marshalling.

#pragma once

#include "kernel/types.h"

#include <cstddef>

namespace kernel::symmetry {

struct DirectionCandidateResult {
  std::size_t count = 0;
  bool capacity_exceeded = false;
};

struct PlaneCandidateResult {
  std::size_t count = 0;
  bool capacity_exceeded = false;
};

DirectionCandidateResult generate_symmetry_candidate_directions(
    const kernel_symmetry_atom_input* atoms,
    std::size_t atom_count,
    const kernel_symmetry_direction_input* principal_axes,
    std::size_t principal_axis_count,
    kernel_symmetry_direction_input* out_directions,
    std::size_t out_direction_capacity);

PlaneCandidateResult generate_symmetry_candidate_planes(
    const kernel_symmetry_atom_input* atoms,
    std::size_t atom_count,
    const kernel_symmetry_axis_input* found_axes,
    std::size_t axis_count,
    const kernel_symmetry_direction_input* principal_axes,
    std::size_t principal_axis_count,
    kernel_symmetry_plane_input* out_planes,
    std::size_t out_plane_capacity);

}  // namespace kernel::symmetry
