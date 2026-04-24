// Reason: This file keeps symmetry molecule text parsing in the kernel while
// hosts only marshal atom arrays into existing symmetry search DTOs.

#pragma once

#include "kernel/types.h"

#include <string>
#include <string_view>
#include <vector>

namespace kernel::symmetry {

struct SymmetryAtom {
  std::string element;
  double position[3]{};
  double mass = 12.0;
};

struct SymmetryAtomParseResult {
  std::vector<SymmetryAtom> atoms;
  kernel_symmetry_parse_error error = KERNEL_SYMMETRY_PARSE_ERROR_NONE;
};

SymmetryAtomParseResult parse_symmetry_atoms_text(
    std::string_view raw,
    std::string_view format);

}  // namespace kernel::symmetry
