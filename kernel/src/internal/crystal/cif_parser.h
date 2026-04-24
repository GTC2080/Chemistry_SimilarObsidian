// Reason: This file isolates CIF crystal parsing in the kernel while hosts
// keep command DTOs and localized error text.

#pragma once

#include "kernel/types.h"

#include <string>
#include <string_view>
#include <vector>

namespace kernel::crystal {

struct FractionalAtomRecord {
  std::string element;
  double frac[3]{};
};

struct CifParseComputation {
  kernel_crystal_cell_params cell{};
  std::vector<FractionalAtomRecord> atoms;
  std::vector<kernel_symmetry_operation_input> symops;
  kernel_crystal_parse_error error = KERNEL_CRYSTAL_PARSE_ERROR_NONE;
};

CifParseComputation parse_cif_crystal(std::string_view raw);

}  // namespace kernel::crystal
