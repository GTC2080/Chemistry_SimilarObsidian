// Reason: This file isolates molecular text preview construction in the kernel
// while hosts keep file IO and command marshalling.

#pragma once

#include "kernel/types.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace kernel::chemistry {

struct MolecularPreviewComputation {
  std::string preview_data;
  std::size_t atom_count = 0;
  std::size_t preview_atom_count = 0;
  bool truncated = false;
  kernel_molecular_preview_error error = KERNEL_MOLECULAR_PREVIEW_ERROR_NONE;
};

MolecularPreviewComputation build_molecular_preview(
    std::string_view raw,
    std::string_view extension,
    std::size_t max_atoms);

}  // namespace kernel::chemistry
