// Reason: This file keeps stoichiometry numeric rules in the chemistry kernel
// while hosts continue to marshal UI row labels and formulas.

#pragma once

#include "kernel/types.h"

#include <cstddef>

namespace kernel::chemistry {

void recalculate_stoichiometry_rows(
    const kernel_stoichiometry_row_input* rows,
    std::size_t count,
    kernel_stoichiometry_row_output* out_rows);

}  // namespace kernel::chemistry
