// Reason: This file owns symmetry molecule shape analysis formerly computed
// in the Tauri Rust backend.

#include "symmetry/shape_analysis.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string_view>
#include <vector>

namespace kernel::symmetry {
namespace {

constexpr double kTolerance = 0.30;

struct ShapeAtom {
  std::string_view element;
  double pos[3]{};
  double mass = 0.0;
};

double dot(const double a[3], const double b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

double length(const double value[3]) {
  return std::sqrt(dot(value, value));
}

void subtract(const double a[3], const double b[3], double out[3]) {
  out[0] = a[0] - b[0];
  out[1] = a[1] - b[1];
  out[2] = a[2] - b[2];
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

void calculate_center_of_mass(
    const kernel_symmetry_atom_input* atoms,
    const std::size_t atom_count,
    double out_center[3]) {
  double total_mass = 0.0;
  double weighted[3]{};
  for (std::size_t index = 0; index < atom_count; ++index) {
    const auto& atom = atoms[index];
    total_mass += atom.mass;
    weighted[0] += atom.position[0] * atom.mass;
    weighted[1] += atom.position[1] * atom.mass;
    weighted[2] += atom.position[2] * atom.mass;
  }

  if (total_mass >= 1.0e-10) {
    out_center[0] = weighted[0] / total_mass;
    out_center[1] = weighted[1] / total_mass;
    out_center[2] = weighted[2] / total_mass;
    return;
  }

  for (std::size_t index = 0; index < atom_count; ++index) {
    out_center[0] += atoms[index].position[0];
    out_center[1] += atoms[index].position[1];
    out_center[2] += atoms[index].position[2];
  }
  out_center[0] /= static_cast<double>(atom_count);
  out_center[1] /= static_cast<double>(atom_count);
  out_center[2] /= static_cast<double>(atom_count);
}

std::vector<ShapeAtom> center_atoms(
    const kernel_symmetry_atom_input* atoms,
    const std::size_t atom_count,
    const double center[3],
    double& out_radius) {
  std::vector<ShapeAtom> centered;
  centered.reserve(atom_count);
  out_radius = 1.0;

  for (std::size_t index = 0; index < atom_count; ++index) {
    ShapeAtom atom{};
    atom.element = std::string_view(atoms[index].element);
    atom.mass = atoms[index].mass;
    subtract(atoms[index].position, center, atom.pos);
    out_radius = std::max(out_radius, length(atom.pos));
    centered.push_back(atom);
  }

  return centered;
}

bool check_linear(const std::vector<ShapeAtom>& atoms) {
  if (atoms.size() <= 2) {
    return true;
  }

  const auto& p0 = atoms.front().pos;
  double direction_raw[3]{};
  double direction[3]{};
  bool found_direction = false;
  for (std::size_t index = 1; index < atoms.size(); ++index) {
    subtract(atoms[index].pos, p0, direction_raw);
    if (length(direction_raw) > kTolerance) {
      normalize(direction_raw, direction);
      found_direction = true;
      break;
    }
  }
  if (!found_direction) {
    return true;
  }

  for (std::size_t index = 1; index < atoms.size(); ++index) {
    double value[3]{};
    subtract(atoms[index].pos, p0, value);
    const double projection = dot(value, direction);
    const double perpendicular[3] = {
        value[0] - direction[0] * projection,
        value[1] - direction[1] * projection,
        value[2] - direction[2] * projection,
    };
    if (length(perpendicular) >= kTolerance) {
      return false;
    }
  }

  return true;
}

void find_linear_axis(const std::vector<ShapeAtom>& atoms, double out_axis[3]) {
  out_axis[0] = 1.0;
  out_axis[1] = 0.0;
  out_axis[2] = 0.0;
  if (atoms.empty()) {
    return;
  }

  const auto& p0 = atoms.front().pos;
  double direction_raw[3]{};
  for (std::size_t index = 1; index < atoms.size(); ++index) {
    subtract(atoms[index].pos, p0, direction_raw);
    if (length(direction_raw) > kTolerance && normalize(direction_raw, out_axis)) {
      return;
    }
  }
}

bool check_inversion(const std::vector<ShapeAtom>& atoms) {
  std::vector<unsigned char> used(atoms.size(), 0);

  for (const auto& atom : atoms) {
    double best_dist = std::numeric_limits<double>::infinity();
    std::size_t best_index = atoms.size();
    const double transformed[3] = {-atom.pos[0], -atom.pos[1], -atom.pos[2]};

    for (std::size_t index = 0; index < atoms.size(); ++index) {
      const auto& other = atoms[index];
      if (used[index] != 0 || other.element != atom.element) {
        continue;
      }
      double diff[3]{};
      subtract(other.pos, transformed, diff);
      const double dist = length(diff);
      if (dist < kTolerance && dist < best_dist) {
        best_dist = dist;
        best_index = index;
      }
    }

    if (best_index == atoms.size()) {
      return false;
    }
    used[best_index] = 1;
  }

  return true;
}

}  // namespace

kernel_symmetry_shape_result analyze_symmetry_shape(
    const kernel_symmetry_atom_input* atoms,
    const std::size_t atom_count) {
  kernel_symmetry_shape_result result{};
  calculate_center_of_mass(atoms, atom_count, result.center_of_mass);

  double mol_radius = 1.0;
  const auto centered = center_atoms(atoms, atom_count, result.center_of_mass, mol_radius);
  result.mol_radius = mol_radius;
  result.is_linear = check_linear(centered) ? 1 : 0;
  find_linear_axis(centered, result.linear_axis);
  result.has_inversion = check_inversion(centered) ? 1 : 0;
  return result;
}

}  // namespace kernel::symmetry
