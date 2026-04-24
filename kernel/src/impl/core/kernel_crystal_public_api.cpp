// Reason: This file exposes stateless crystal compute helpers through the
// kernel C ABI so Tauri Rust no longer owns Miller-plane math.

#include "kernel/c_api.h"

#include "core/kernel_shared.h"
#include "crystal/miller_plane.h"

#include <cstring>

namespace {

void reset_miller_plane_result(kernel_miller_plane_result* result) {
  if (result == nullptr) {
    return;
  }
  std::memset(result, 0, sizeof(kernel_miller_plane_result));
  result->error = KERNEL_CRYSTAL_MILLER_ERROR_NONE;
}

}  // namespace

extern "C" kernel_status kernel_calculate_miller_plane(
    const kernel_crystal_cell_params* cell,
    const int32_t h,
    const int32_t k,
    const int32_t l,
    kernel_miller_plane_result* out_result) {
  reset_miller_plane_result(out_result);
  if (cell == nullptr || out_result == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const auto computation = kernel::crystal::calculate_miller_plane(*cell, h, k, l);
  if (computation.error != KERNEL_CRYSTAL_MILLER_ERROR_NONE) {
    out_result->error = computation.error;
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  *out_result = computation.result;
  return kernel::core::make_status(KERNEL_OK);
}
