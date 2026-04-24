// Reason: This file owns symmetry candidate direction/plane generation
// formerly implemented in the Tauri Rust backend.

#include "symmetry/candidate_generation.h"

#include <algorithm>
#include <cmath>
#include <string_view>
#include <vector>

namespace kernel::symmetry {
namespace {

constexpr double kTolerance = 0.30;
constexpr double kAngleTolerance = 0.10;

struct CandidateAtom {
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

void copy_vec(const double source[3], double out[3]) {
  out[0] = source[0];
  out[1] = source[1];
  out[2] = source[2];
}

bool are_parallel(const double a[3], const double b[3]) {
  return std::abs(dot(a, b)) > std::cos(kAngleTolerance);
}

std::vector<CandidateAtom> to_candidate_atoms(
    const kernel_symmetry_atom_input* atoms,
    const std::size_t atom_count) {
  std::vector<CandidateAtom> result;
  result.reserve(atom_count);
  for (std::size_t index = 0; index < atom_count; ++index) {
    CandidateAtom atom{};
    atom.element = std::string_view(atoms[index].element);
    copy_vec(atoms[index].position, atom.pos);
    result.push_back(atom);
  }
  return result;
}

bool add_unique_direction(
    kernel_symmetry_direction_input* out_directions,
    const std::size_t capacity,
    std::size_t& count,
    const double direction[3]) {
  for (std::size_t index = 0; index < count; ++index) {
    if (are_parallel(out_directions[index].dir, direction)) {
      return true;
    }
  }
  if (count >= capacity) {
    return false;
  }
  copy_vec(direction, out_directions[count].dir);
  ++count;
  return true;
}

bool add_unique_plane(
    kernel_symmetry_plane_input* out_planes,
    const std::size_t capacity,
    std::size_t& count,
    const double normal[3]) {
  for (std::size_t index = 0; index < count; ++index) {
    if (are_parallel(out_planes[index].normal, normal)) {
      return true;
    }
  }
  if (count >= capacity) {
    return false;
  }
  copy_vec(normal, out_planes[count].normal);
  ++count;
  return true;
}

}  // namespace

DirectionCandidateResult generate_symmetry_candidate_directions(
    const kernel_symmetry_atom_input* atoms,
    const std::size_t atom_count,
    const kernel_symmetry_direction_input* principal_axes,
    const std::size_t principal_axis_count,
    kernel_symmetry_direction_input* out_directions,
    const std::size_t out_direction_capacity) {
  DirectionCandidateResult result{};
  const auto candidate_atoms = to_candidate_atoms(atoms, atom_count);
  auto add = [&](const double direction[3]) {
    if (!add_unique_direction(out_directions, out_direction_capacity, result.count, direction)) {
      result.capacity_exceeded = true;
    }
  };

  for (std::size_t index = 0; index < principal_axis_count && !result.capacity_exceeded; ++index) {
    double normalized[3]{};
    if (normalize(principal_axes[index].dir, normalized)) {
      add(normalized);
    }
  }

  const double unit_axes[3][3] = {{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}};
  for (const auto& axis : unit_axes) {
    if (result.capacity_exceeded) {
      return result;
    }
    add(axis);
  }

  for (const auto& atom : candidate_atoms) {
    if (result.capacity_exceeded) {
      return result;
    }
    double normalized[3]{};
    if (length(atom.pos) > kTolerance && normalize(atom.pos, normalized)) {
      add(normalized);
    }
  }

  for (std::size_t i = 0; i < candidate_atoms.size(); ++i) {
    for (std::size_t j = i + 1; j < candidate_atoms.size(); ++j) {
      if (result.capacity_exceeded) {
        return result;
      }
      if (candidate_atoms[i].element != candidate_atoms[j].element) {
        continue;
      }

      const double midpoint[3] = {
          (candidate_atoms[i].pos[0] + candidate_atoms[j].pos[0]) / 2.0,
          (candidate_atoms[i].pos[1] + candidate_atoms[j].pos[1]) / 2.0,
          (candidate_atoms[i].pos[2] + candidate_atoms[j].pos[2]) / 2.0,
      };
      double normalized_midpoint[3]{};
      if (length(midpoint) > kTolerance * 0.5 && normalize(midpoint, normalized_midpoint)) {
        add(normalized_midpoint);
      }

      const double diff[3] = {
          candidate_atoms[i].pos[0] - candidate_atoms[j].pos[0],
          candidate_atoms[i].pos[1] - candidate_atoms[j].pos[1],
          candidate_atoms[i].pos[2] - candidate_atoms[j].pos[2],
      };
      double normalized_diff[3]{};
      if (length(diff) > kTolerance && normalize(diff, normalized_diff)) {
        add(normalized_diff);
      }
    }
  }

  std::vector<kernel_symmetry_direction_input> atom_dirs;
  atom_dirs.reserve(candidate_atoms.size());
  for (const auto& atom : candidate_atoms) {
    double normalized[3]{};
    if (length(atom.pos) > kTolerance && normalize(atom.pos, normalized)) {
      kernel_symmetry_direction_input direction{};
      copy_vec(normalized, direction.dir);
      atom_dirs.push_back(direction);
    }
  }

  const std::size_t cross_limit = std::min<std::size_t>(atom_dirs.size(), 20);
  for (std::size_t i = 0; i < cross_limit; ++i) {
    for (std::size_t j = i + 1; j < cross_limit; ++j) {
      if (result.capacity_exceeded) {
        return result;
      }
      double cross_value[3]{};
      cross(atom_dirs[i].dir, atom_dirs[j].dir, cross_value);
      double normalized[3]{};
      if (length(cross_value) > 1.0e-6 && normalize(cross_value, normalized)) {
        add(normalized);
      }
    }
  }

  return result;
}

PlaneCandidateResult generate_symmetry_candidate_planes(
    const kernel_symmetry_atom_input* atoms,
    const std::size_t atom_count,
    const kernel_symmetry_axis_input* found_axes,
    const std::size_t axis_count,
    const kernel_symmetry_direction_input* principal_axes,
    const std::size_t principal_axis_count,
    kernel_symmetry_plane_input* out_planes,
    const std::size_t out_plane_capacity) {
  PlaneCandidateResult result{};
  const auto candidate_atoms = to_candidate_atoms(atoms, atom_count);
  auto add = [&](const double normal[3]) {
    if (!add_unique_plane(out_planes, out_plane_capacity, result.count, normal)) {
      result.capacity_exceeded = true;
    }
  };

  for (std::size_t index = 0; index < axis_count && !result.capacity_exceeded; ++index) {
    add(found_axes[index].dir);
  }
  for (std::size_t index = 0; index < principal_axis_count && !result.capacity_exceeded; ++index) {
    double normalized[3]{};
    if (normalize(principal_axes[index].dir, normalized)) {
      add(normalized);
    }
  }

  const double unit_axes[3][3] = {{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}};
  for (const auto& axis : unit_axes) {
    if (result.capacity_exceeded) {
      return result;
    }
    add(axis);
  }

  for (const auto& atom : candidate_atoms) {
    if (result.capacity_exceeded) {
      return result;
    }
    double normalized[3]{};
    if (length(atom.pos) > kTolerance && normalize(atom.pos, normalized)) {
      add(normalized);
    }
  }

  for (std::size_t i = 0; i < candidate_atoms.size(); ++i) {
    for (std::size_t j = i + 1; j < candidate_atoms.size(); ++j) {
      if (result.capacity_exceeded) {
        return result;
      }
      if (candidate_atoms[i].element != candidate_atoms[j].element) {
        continue;
      }

      const double diff[3] = {
          candidate_atoms[i].pos[0] - candidate_atoms[j].pos[0],
          candidate_atoms[i].pos[1] - candidate_atoms[j].pos[1],
          candidate_atoms[i].pos[2] - candidate_atoms[j].pos[2],
      };
      double normalized_diff[3]{};
      if (length(diff) > kTolerance && normalize(diff, normalized_diff)) {
        add(normalized_diff);
      }

      const double midpoint[3] = {
          (candidate_atoms[i].pos[0] + candidate_atoms[j].pos[0]) / 2.0,
          (candidate_atoms[i].pos[1] + candidate_atoms[j].pos[1]) / 2.0,
          (candidate_atoms[i].pos[2] + candidate_atoms[j].pos[2]) / 2.0,
      };
      double normalized_midpoint[3]{};
      if (length(midpoint) > kTolerance * 0.5 && normalize(midpoint, normalized_midpoint)) {
        add(normalized_midpoint);
      }
    }
  }

  for (std::size_t axis_index = 0; axis_index < axis_count; ++axis_index) {
    for (const auto& atom : candidate_atoms) {
      if (result.capacity_exceeded) {
        return result;
      }
      double cross_value[3]{};
      cross(found_axes[axis_index].dir, atom.pos, cross_value);
      double normalized[3]{};
      if (length(atom.pos) > kTolerance && length(cross_value) > 1.0e-6 &&
          normalize(cross_value, normalized)) {
        add(normalized);
      }
    }
  }

  return result;
}

}  // namespace kernel::symmetry
