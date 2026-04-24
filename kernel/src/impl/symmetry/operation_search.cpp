// Reason: This file owns symmetry operation matching formerly implemented
// in the Tauri Rust backend.

#include "symmetry/operation_search.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <string_view>
#include <vector>

namespace kernel::symmetry {
namespace {

constexpr double kTolerance = 0.30;
constexpr double kAngleTolerance = 0.10;

struct SearchAtom {
  std::string_view element;
  double pos[3]{};
};

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

void subtract(const double a[3], const double b[3], double out[3]) {
  out[0] = a[0] - b[0];
  out[1] = a[1] - b[1];
  out[2] = a[2] - b[2];
}

void copy_vec(const double source[3], double out[3]) {
  out[0] = source[0];
  out[1] = source[1];
  out[2] = source[2];
}

bool are_parallel(const double a[3], const double b[3]) {
  return std::abs(dot(a, b)) > std::cos(kAngleTolerance);
}

std::vector<SearchAtom> to_search_atoms(
    const kernel_symmetry_atom_input* atoms,
    const std::size_t atom_count) {
  std::vector<SearchAtom> result;
  result.reserve(atom_count);
  for (std::size_t index = 0; index < atom_count; ++index) {
    SearchAtom atom{};
    atom.element = std::string_view(atoms[index].element);
    copy_vec(atoms[index].position, atom.pos);
    result.push_back(atom);
  }
  return result;
}

void rotate_point(
    const double value[3],
    const double axis[3],
    const double angle,
    double out[3]) {
  const double cos_a = std::cos(angle);
  const double sin_a = std::sin(angle);
  double axis_cross_value[3]{};
  cross(axis, value, axis_cross_value);
  const double axis_dot_value = dot(axis, value);
  for (int coord = 0; coord < 3; ++coord) {
    out[coord] = value[coord] * cos_a + axis_cross_value[coord] * sin_a +
        axis[coord] * axis_dot_value * (1.0 - cos_a);
  }
}

void reflect_point(const double value[3], const double normal[3], double out[3]) {
  const double factor = 2.0 * dot(value, normal);
  for (int coord = 0; coord < 3; ++coord) {
    out[coord] = value[coord] - normal[coord] * factor;
  }
}

template <typename Transform>
bool check_operation(const std::vector<SearchAtom>& atoms, Transform transform) {
  std::vector<unsigned char> used(atoms.size(), 0);

  for (const auto& atom : atoms) {
    double transformed[3]{};
    transform(atom.pos, transformed);
    std::size_t best_index = atoms.size();
    double best_dist = std::numeric_limits<double>::infinity();

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

bool has_matching_axis(
    const kernel_symmetry_axis_input* axes,
    const std::size_t count,
    const double direction[3],
    const uint8_t order) {
  for (std::size_t index = 0; index < count; ++index) {
    if (axes[index].order == order && are_parallel(axes[index].dir, direction)) {
      return true;
    }
  }
  return false;
}

bool has_matching_plane(
    const kernel_symmetry_plane_input* planes,
    const std::size_t count,
    const double normal[3]) {
  for (std::size_t index = 0; index < count; ++index) {
    if (are_parallel(planes[index].normal, normal)) {
      return true;
    }
  }
  return false;
}

}  // namespace

RotationAxisSearchResult find_symmetry_rotation_axes(
    const kernel_symmetry_atom_input* atoms,
    const std::size_t atom_count,
    const kernel_symmetry_direction_input* candidates,
    const std::size_t candidate_count,
    kernel_symmetry_axis_input* out_axes,
    const std::size_t out_axis_capacity) {
  RotationAxisSearchResult result{};
  const auto search_atoms = to_search_atoms(atoms, atom_count);
  constexpr uint8_t orders[] = {6, 5, 4, 3, 2};

  for (std::size_t candidate_index = 0; candidate_index < candidate_count; ++candidate_index) {
    const double* direction = candidates[candidate_index].dir;
    for (const uint8_t order : orders) {
      const double angle = 2.0 * std::numbers::pi / static_cast<double>(order);
      const bool is_rotation = check_operation(
          search_atoms,
          [direction, angle](const double value[3], double out[3]) {
            rotate_point(value, direction, angle, out);
          });
      if (!is_rotation || has_matching_axis(out_axes, result.count, direction, order)) {
        continue;
      }
      if (result.count >= out_axis_capacity) {
        result.capacity_exceeded = true;
        return result;
      }
      copy_vec(direction, out_axes[result.count].dir);
      out_axes[result.count].order = order;
      ++result.count;
    }
  }

  std::sort(out_axes, out_axes + result.count, [](const auto& lhs, const auto& rhs) {
    return lhs.order > rhs.order;
  });
  return result;
}

MirrorPlaneSearchResult find_symmetry_mirror_planes(
    const kernel_symmetry_atom_input* atoms,
    const std::size_t atom_count,
    const kernel_symmetry_plane_input* candidates,
    const std::size_t candidate_count,
    kernel_symmetry_plane_input* out_planes,
    const std::size_t out_plane_capacity) {
  MirrorPlaneSearchResult result{};
  const auto search_atoms = to_search_atoms(atoms, atom_count);

  for (std::size_t candidate_index = 0; candidate_index < candidate_count; ++candidate_index) {
    const double* normal = candidates[candidate_index].normal;
    const bool is_mirror = check_operation(
        search_atoms,
        [normal](const double value[3], double out[3]) {
          reflect_point(value, normal, out);
        });
    if (!is_mirror || has_matching_plane(out_planes, result.count, normal)) {
      continue;
    }
    if (result.count >= out_plane_capacity) {
      result.capacity_exceeded = true;
      return result;
    }
    copy_vec(normal, out_planes[result.count].normal);
    ++result.count;
  }

  return result;
}

}  // namespace kernel::symmetry
