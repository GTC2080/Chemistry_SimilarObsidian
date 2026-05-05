// Reason: This file exposes stoichiometry recalculation through the kernel C
// ABI so Tauri Rust no longer owns chemistry calculation rules.

#include "kernel/c_api.h"

#include "chemistry/stoichiometry.h"
#include "core/kernel_shared.h"

extern "C" kernel_status kernel_recalculate_stoichiometry(
    const kernel_stoichiometry_row_input* rows,
    const size_t count,
    kernel_stoichiometry_row_output* out_rows) {
  if (count == 0) {
    return kernel::core::make_status(KERNEL_OK);
  }
  if (rows == nullptr || out_rows == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel::chemistry::recalculate_stoichiometry_rows(rows, count, out_rows);
  return kernel::core::make_status(KERNEL_OK);
}
