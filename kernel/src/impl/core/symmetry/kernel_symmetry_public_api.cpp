// Reason: This file exposes stateless symmetry classification through the
// kernel C ABI so Tauri Rust no longer owns point-group rules.

#include "kernel/c_api.h"

#include "core/kernel_shared.h"
#include "symmetry/classify.h"

#include <cstring>

extern "C" kernel_status kernel_classify_point_group(
    const kernel_symmetry_axis_input* axes,
    const size_t axis_count,
    const kernel_symmetry_plane_input* planes,
    const size_t plane_count,
    const uint8_t has_inversion,
    kernel_symmetry_classification_result* out_result) {
  if (out_result != nullptr) {
    std::memset(out_result, 0, sizeof(kernel_symmetry_classification_result));
  }
  if (out_result == nullptr || (axis_count > 0 && axes == nullptr) ||
      (plane_count > 0 && planes == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string point_group = kernel::symmetry::classify_point_group(
      axes,
      axis_count,
      planes,
      plane_count,
      has_inversion != 0);
  if (point_group.size() >= KERNEL_SYMMETRY_POINT_GROUP_MAX) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }

  std::memcpy(out_result->point_group, point_group.c_str(), point_group.size() + 1);
  return kernel::core::make_status(KERNEL_OK);
}
