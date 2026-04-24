// Reason: This file exposes symmetry render geometry through the kernel C ABI
// so Tauri Rust no longer owns axis endpoint and mirror-plane vertex math.

#include "kernel/c_api.h"

#include "core/kernel_shared.h"
#include "symmetry/render_geometry.h"

#include <cmath>

extern "C" kernel_status kernel_build_symmetry_render_geometry(
    const kernel_symmetry_axis_input* axes,
    const size_t axis_count,
    const kernel_symmetry_plane_input* planes,
    const size_t plane_count,
    const double mol_radius,
    kernel_symmetry_render_axis* out_axes,
    kernel_symmetry_render_plane* out_planes) {
  if ((axis_count > 0 && (axes == nullptr || out_axes == nullptr)) ||
      (plane_count > 0 && (planes == nullptr || out_planes == nullptr)) ||
      !std::isfinite(mol_radius) || mol_radius < 0.0) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel::symmetry::build_symmetry_render_geometry(
      axes,
      axis_count,
      planes,
      plane_count,
      mol_radius,
      out_axes,
      out_planes);
  return kernel::core::make_status(KERNEL_OK);
}
