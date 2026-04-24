// Reason: This file owns symmetry render geometry formerly assembled in the
// Tauri Rust backend.

#include "symmetry/render_geometry.h"

#include <cmath>

namespace kernel::symmetry {
namespace {

double dot(const double a[3], const double b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

void cross(const double a[3], const double b[3], double out[3]) {
  out[0] = a[1] * b[2] - a[2] * b[1];
  out[1] = a[2] * b[0] - a[0] * b[2];
  out[2] = a[0] * b[1] - a[1] * b[0];
}

double length(const double value[3]) {
  return std::sqrt(dot(value, value));
}

bool normalize(const double value[3], double out[3]) {
  const double len = length(value);
  if (len < 1.0e-12 || !std::isfinite(len)) {
    return false;
  }
  out[0] = value[0] / len;
  out[1] = value[1] / len;
  out[2] = value[2] / len;
  return true;
}

void clear_vec(double out[3]) {
  out[0] = 0.0;
  out[1] = 0.0;
  out[2] = 0.0;
}

void copy_vec(const double source[3], double out[3]) {
  out[0] = source[0];
  out[1] = source[1];
  out[2] = source[2];
}

void find_perpendicular(const double value[3], double out[3]) {
  const double candidate[3] = {
      std::abs(value[0]) < 0.9 ? 1.0 : 0.0,
      std::abs(value[0]) < 0.9 ? 0.0 : 1.0,
      0.0,
  };
  cross(value, candidate, out);
  if (length(out) < 1.0e-10) {
    const double z_axis[3] = {0.0, 0.0, 1.0};
    cross(value, z_axis, out);
  }
}

void fill_axis(
    const kernel_symmetry_axis_input& source,
    const double mol_radius,
    kernel_symmetry_render_axis& target) {
  const double extend = mol_radius * 1.5;
  copy_vec(source.dir, target.vector);
  clear_vec(target.center);
  target.order = source.order;
  for (int index = 0; index < 3; ++index) {
    target.start[index] = -source.dir[index] * extend;
    target.end[index] = source.dir[index] * extend;
  }
}

void fill_plane(
    const kernel_symmetry_plane_input& source,
    const double mol_radius,
    kernel_symmetry_render_plane& target) {
  const double size = mol_radius * 1.8;
  copy_vec(source.normal, target.normal);
  clear_vec(target.center);

  double u_raw[3]{};
  double u[3]{};
  find_perpendicular(source.normal, u_raw);
  if (!normalize(u_raw, u)) {
    u[0] = 1.0;
    u[1] = 0.0;
    u[2] = 0.0;
  }

  double v_raw[3]{};
  double v[3]{};
  cross(source.normal, u, v_raw);
  if (!normalize(v_raw, v)) {
    v[0] = 0.0;
    v[1] = 1.0;
    v[2] = 0.0;
  }

  const double signs[4][2] = {{1.0, 1.0}, {1.0, -1.0}, {-1.0, -1.0}, {-1.0, 1.0}};
  for (int vertex = 0; vertex < 4; ++vertex) {
    for (int coord = 0; coord < 3; ++coord) {
      target.vertices[vertex][coord] =
          signs[vertex][0] * u[coord] * size + signs[vertex][1] * v[coord] * size;
    }
  }
}

}  // namespace

void build_symmetry_render_geometry(
    const kernel_symmetry_axis_input* axes,
    const std::size_t axis_count,
    const kernel_symmetry_plane_input* planes,
    const std::size_t plane_count,
    const double mol_radius,
    kernel_symmetry_render_axis* out_axes,
    kernel_symmetry_render_plane* out_planes) {
  for (std::size_t index = 0; index < axis_count; ++index) {
    fill_axis(axes[index], mol_radius, out_axes[index]);
  }
  for (std::size_t index = 0; index < plane_count; ++index) {
    fill_plane(planes[index], mol_radius, out_planes[index]);
  }
}

}  // namespace kernel::symmetry
