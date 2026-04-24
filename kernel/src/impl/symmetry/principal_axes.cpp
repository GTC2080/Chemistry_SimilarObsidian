// Reason: This file owns mass-weighted symmetry principal-axis calculation
// formerly implemented in the Tauri Rust backend.

#include "symmetry/principal_axes.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace kernel::symmetry {
namespace {

using Matrix3 = std::array<std::array<double, 3>, 3>;

double dot(const double a[3], const double b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

double length(const double value[3]) {
  return std::sqrt(dot(value, value));
}

bool normalize(double value[3]) {
  const double len = length(value);
  if (len < 1.0e-12 || !std::isfinite(len)) {
    return false;
  }
  value[0] /= len;
  value[1] /= len;
  value[2] /= len;
  return true;
}

Matrix3 identity_matrix() {
  Matrix3 matrix{};
  matrix[0][0] = 1.0;
  matrix[1][1] = 1.0;
  matrix[2][2] = 1.0;
  return matrix;
}

Matrix3 build_inertia_tensor(
    const kernel_symmetry_atom_input* atoms,
    const std::size_t atom_count) {
  Matrix3 inertia{};
  for (std::size_t index = 0; index < atom_count; ++index) {
    const auto& atom = atoms[index];
    const double x = atom.position[0];
    const double y = atom.position[1];
    const double z = atom.position[2];
    const double mass = atom.mass;
    const double r2 = x * x + y * y + z * z;

    inertia[0][0] += mass * (r2 - x * x);
    inertia[1][1] += mass * (r2 - y * y);
    inertia[2][2] += mass * (r2 - z * z);
    inertia[0][1] -= mass * x * y;
    inertia[1][0] -= mass * x * y;
    inertia[0][2] -= mass * x * z;
    inertia[2][0] -= mass * x * z;
    inertia[1][2] -= mass * y * z;
    inertia[2][1] -= mass * y * z;
  }
  return inertia;
}

void jacobi_rotate(Matrix3& matrix, Matrix3& eigenvectors, const int p, const int q) {
  const double apq = matrix[p][q];
  if (std::abs(apq) < 1.0e-15) {
    return;
  }

  const double tau = (matrix[q][q] - matrix[p][p]) / (2.0 * apq);
  const double sign = tau >= 0.0 ? 1.0 : -1.0;
  const double t = sign / (std::abs(tau) + std::sqrt(1.0 + tau * tau));
  const double c = 1.0 / std::sqrt(1.0 + t * t);
  const double s = t * c;

  for (int k = 0; k < 3; ++k) {
    if (k == p || k == q) {
      continue;
    }
    const double akp = matrix[k][p];
    const double akq = matrix[k][q];
    matrix[k][p] = c * akp - s * akq;
    matrix[p][k] = matrix[k][p];
    matrix[k][q] = s * akp + c * akq;
    matrix[q][k] = matrix[k][q];
  }

  const double app = matrix[p][p];
  const double aqq = matrix[q][q];
  matrix[p][p] = c * c * app - 2.0 * s * c * apq + s * s * aqq;
  matrix[q][q] = s * s * app + 2.0 * s * c * apq + c * c * aqq;
  matrix[p][q] = 0.0;
  matrix[q][p] = 0.0;

  for (int k = 0; k < 3; ++k) {
    const double vkp = eigenvectors[k][p];
    const double vkq = eigenvectors[k][q];
    eigenvectors[k][p] = c * vkp - s * vkq;
    eigenvectors[k][q] = s * vkp + c * vkq;
  }
}

Matrix3 diagonalize_symmetric(Matrix3 matrix, std::array<double, 3>& out_eigenvalues) {
  Matrix3 eigenvectors = identity_matrix();

  for (int iteration = 0; iteration < 48; ++iteration) {
    int p = 0;
    int q = 1;
    double largest = std::abs(matrix[0][1]);
    if (std::abs(matrix[0][2]) > largest) {
      p = 0;
      q = 2;
      largest = std::abs(matrix[0][2]);
    }
    if (std::abs(matrix[1][2]) > largest) {
      p = 1;
      q = 2;
      largest = std::abs(matrix[1][2]);
    }
    if (largest < 1.0e-12) {
      break;
    }
    jacobi_rotate(matrix, eigenvectors, p, q);
  }

  out_eigenvalues = {matrix[0][0], matrix[1][1], matrix[2][2]};
  return eigenvectors;
}

}  // namespace

void compute_symmetry_principal_axes(
    const kernel_symmetry_atom_input* atoms,
    const std::size_t atom_count,
    kernel_symmetry_direction_input out_axes[3]) {
  const Matrix3 inertia = build_inertia_tensor(atoms, atom_count);
  std::array<double, 3> eigenvalues{};
  const Matrix3 eigenvectors = diagonalize_symmetric(inertia, eigenvalues);
  std::array<int, 3> order = {0, 1, 2};
  std::sort(order.begin(), order.end(), [&](const int lhs, const int rhs) {
    return eigenvalues[lhs] < eigenvalues[rhs];
  });

  for (int output_index = 0; output_index < 3; ++output_index) {
    const int source_column = order[output_index];
    double axis[3] = {
        eigenvectors[0][source_column],
        eigenvectors[1][source_column],
        eigenvectors[2][source_column],
    };
    if (!normalize(axis)) {
      axis[0] = output_index == 0 ? 1.0 : 0.0;
      axis[1] = output_index == 1 ? 1.0 : 0.0;
      axis[2] = output_index == 2 ? 1.0 : 0.0;
    }
    out_axes[output_index].dir[0] = axis[0];
    out_axes[output_index].dir[1] = axis[1];
    out_axes[output_index].dir[2] = axis[2];
  }
}

}  // namespace kernel::symmetry
