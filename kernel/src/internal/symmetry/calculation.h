// Reason: This file owns the end-to-end stateless symmetry calculation
// pipeline so hosts do not orchestrate parser/search/classification stages.

#pragma once

#include "kernel/types.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace kernel::symmetry {

struct SymmetryCalculation {
  std::string point_group;
  std::vector<kernel_symmetry_render_axis> axes;
  std::vector<kernel_symmetry_render_plane> planes;
  bool has_inversion = false;
  std::size_t atom_count = 0;
  kernel_symmetry_calculation_error error = KERNEL_SYMMETRY_CALC_ERROR_NONE;
  kernel_symmetry_parse_error parse_error = KERNEL_SYMMETRY_PARSE_ERROR_NONE;
};

SymmetryCalculation calculate_symmetry(
    std::string_view raw,
    std::string_view format,
    std::size_t max_atoms);

}  // namespace kernel::symmetry
