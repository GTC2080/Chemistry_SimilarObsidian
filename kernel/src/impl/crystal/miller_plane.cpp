// Reason: This file owns Miller-plane numeric geometry formerly implemented
// in the Tauri Rust backend.

#include "crystal/miller_plane.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace kernel::crystal {
namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;

struct LatticeVectors {
  double vectors[3][3]{};
  kernel_crystal_miller_error error = KERNEL_CRYSTAL_MILLER_ERROR_NONE;
};

double radians(const double degrees) {
  return degrees * kPi / 180.0;
}

double dot(const double a[3], const double b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

void cross(const double a[3], const double b[3], double out[3]) {
  out[0] = a[1] * b[2] - a[2] * b[1];
  out[1] = a[2] * b[0] - a[0] * b[2];
  out[2] = a[0] * b[1] - a[1] * b[0];
}

void scale(const double value[3], const double factor, double out[3]) {
  out[0] = value[0] * factor;
  out[1] = value[1] * factor;
  out[2] = value[2] * factor;
}

double length(const double value[3]) {
  return std::sqrt(dot(value, value));
}

LatticeVectors build_lattice_vectors(const kernel_crystal_cell_params& cell) {
  LatticeVectors result{};

  const double alpha = radians(cell.alpha_deg);
  const double beta = radians(cell.beta_deg);
  const double gamma = radians(cell.gamma_deg);

  const double cos_alpha = std::cos(alpha);
  const double cos_beta = std::cos(beta);
  const double cos_gamma = std::cos(gamma);
  const double sin_gamma = std::sin(gamma);

  if (std::abs(sin_gamma) < 1.0e-8) {
    result.error = KERNEL_CRYSTAL_MILLER_ERROR_GAMMA_TOO_SMALL;
    return result;
  }

  const double ax = cell.a;
  const double bx = cell.b * cos_gamma;
  const double by = cell.b * sin_gamma;
  const double cx = cell.c * cos_beta;
  const double cy = cell.c * (cos_alpha - cos_beta * cos_gamma) / sin_gamma;
  const double cz2 = cell.c * cell.c - cx * cx - cy * cy;
  if (cz2 < -1.0e-8) {
    result.error = KERNEL_CRYSTAL_MILLER_ERROR_INVALID_BASIS;
    return result;
  }
  const double cz = std::sqrt(std::max(0.0, cz2));

  result.vectors[0][0] = ax;
  result.vectors[0][1] = 0.0;
  result.vectors[0][2] = 0.0;
  result.vectors[1][0] = bx;
  result.vectors[1][1] = by;
  result.vectors[1][2] = 0.0;
  result.vectors[2][0] = cx;
  result.vectors[2][1] = cy;
  result.vectors[2][2] = cz;
  return result;
}

void fill_plane_vertices(
    const double normal[3],
    const double center[3],
    const kernel_crystal_cell_params& cell,
    double out_vertices[4][3]) {
  const double radius = std::max({cell.a, cell.b, cell.c}) * 1.2;
  const double ref_vec[3] =
      {std::abs(normal[0]) < 0.9 ? 1.0 : 0.0,
       std::abs(normal[0]) < 0.9 ? 0.0 : 1.0,
       0.0};

  double u_raw[3]{};
  cross(normal, ref_vec, u_raw);
  const double u_len = length(u_raw);
  const double u[3] = {u_raw[0] / u_len, u_raw[1] / u_len, u_raw[2] / u_len};

  double v[3]{};
  cross(normal, u, v);

  const double signs[4][2] = {{-1.0, -1.0}, {1.0, -1.0}, {1.0, 1.0}, {-1.0, 1.0}};
  for (int index = 0; index < 4; ++index) {
    out_vertices[index][0] =
        center[0] + signs[index][0] * radius * u[0] + signs[index][1] * radius * v[0];
    out_vertices[index][1] =
        center[1] + signs[index][0] * radius * u[1] + signs[index][1] * radius * v[1];
    out_vertices[index][2] =
        center[2] + signs[index][0] * radius * u[2] + signs[index][1] * radius * v[2];
  }
}

}  // namespace

MillerPlaneComputation calculate_miller_plane(
    const kernel_crystal_cell_params& cell,
    const int h,
    const int k,
    const int l) {
  MillerPlaneComputation computation{};
  computation.result.error = KERNEL_CRYSTAL_MILLER_ERROR_NONE;

  if (h == 0 && k == 0 && l == 0) {
    computation.error = KERNEL_CRYSTAL_MILLER_ERROR_ZERO_INDEX;
    return computation;
  }

  const LatticeVectors lattice = build_lattice_vectors(cell);
  if (lattice.error != KERNEL_CRYSTAL_MILLER_ERROR_NONE) {
    computation.error = lattice.error;
    return computation;
  }

  const double* va = lattice.vectors[0];
  const double* vb = lattice.vectors[1];
  const double* vc = lattice.vectors[2];

  double vb_cross_vc[3]{};
  cross(vb, vc, vb_cross_vc);
  const double vol = dot(va, vb_cross_vc);
  if (std::abs(vol) < 1.0e-12) {
    computation.error = KERNEL_CRYSTAL_MILLER_ERROR_ZERO_VOLUME;
    return computation;
  }

  double vc_cross_va[3]{};
  double va_cross_vb[3]{};
  cross(vc, va, vc_cross_va);
  cross(va, vb, va_cross_vb);

  double recip_a[3]{};
  double recip_b[3]{};
  double recip_c[3]{};
  scale(vb_cross_vc, 1.0 / vol, recip_a);
  scale(vc_cross_va, 1.0 / vol, recip_b);
  scale(va_cross_vb, 1.0 / vol, recip_c);

  double normal[3] = {
      static_cast<double>(h) * recip_a[0] + static_cast<double>(k) * recip_b[0] +
          static_cast<double>(l) * recip_c[0],
      static_cast<double>(h) * recip_a[1] + static_cast<double>(k) * recip_b[1] +
          static_cast<double>(l) * recip_c[1],
      static_cast<double>(h) * recip_a[2] + static_cast<double>(k) * recip_b[2] +
          static_cast<double>(l) * recip_c[2],
  };

  const double norm_len = length(normal);
  if (norm_len < 1.0e-12) {
    computation.error = KERNEL_CRYSTAL_MILLER_ERROR_ZERO_NORMAL;
    return computation;
  }

  auto& result = computation.result;
  for (int index = 0; index < 3; ++index) {
    result.normal[index] = normal[index] / norm_len;
  }

  const double d_spacing = 1.0 / norm_len;
  result.d = -d_spacing;
  for (int index = 0; index < 3; ++index) {
    result.center[index] = result.normal[index] * d_spacing;
  }
  fill_plane_vertices(result.normal, result.center, cell, result.vertices);

  result.error = KERNEL_CRYSTAL_MILLER_ERROR_NONE;
  computation.error = KERNEL_CRYSTAL_MILLER_ERROR_NONE;
  return computation;
}

}  // namespace kernel::crystal
