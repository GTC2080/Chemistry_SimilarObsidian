// Reason: This file composes symmetry parser, shape analysis, principal axes,
// candidate generation, operation search, classification, and render geometry
// inside the kernel instead of the Tauri Rust backend.

#include "symmetry/calculation.h"

#include "symmetry/atom_parser.h"
#include "symmetry/candidate_generation.h"
#include "symmetry/classify.h"
#include "symmetry/operation_search.h"
#include "symmetry/principal_axes.h"
#include "symmetry/render_geometry.h"
#include "symmetry/shape_analysis.h"

#include <algorithm>
#include <string>
#include <vector>

namespace kernel::symmetry {
namespace {

std::string d_infinity_h() {
  return std::string("D") + "\xE2\x88\x9E" + "h";
}

std::string c_infinity_v() {
  return std::string("C") + "\xE2\x88\x9E" + "v";
}

std::vector<kernel_symmetry_atom_input> to_atom_inputs(
    const std::vector<SymmetryAtom>& atoms) {
  std::vector<kernel_symmetry_atom_input> inputs;
  inputs.reserve(atoms.size());
  for (const auto& atom : atoms) {
    kernel_symmetry_atom_input input{};
    input.element = atom.element.c_str();
    input.position[0] = atom.position[0];
    input.position[1] = atom.position[1];
    input.position[2] = atom.position[2];
    input.mass = atom.mass;
    inputs.push_back(input);
  }
  return inputs;
}

void center_atoms(
    std::vector<kernel_symmetry_atom_input>& atoms,
    const double center_of_mass[3]) {
  for (auto& atom : atoms) {
    atom.position[0] -= center_of_mass[0];
    atom.position[1] -= center_of_mass[1];
    atom.position[2] -= center_of_mass[2];
  }
}

std::size_t direction_candidate_capacity(const std::size_t atom_count) {
  return std::max<std::size_t>(1, 6 + atom_count + atom_count * (atom_count - 1) + 190);
}

std::size_t plane_candidate_capacity(
    const std::size_t atom_count,
    const std::size_t axis_count) {
  return std::max<std::size_t>(
      1,
      axis_count + 6 + atom_count + atom_count * (atom_count - 1) + axis_count * atom_count);
}

SymmetryCalculation internal_error() {
  SymmetryCalculation result;
  result.error = KERNEL_SYMMETRY_CALC_ERROR_INTERNAL;
  return result;
}

}  // namespace

SymmetryCalculation calculate_symmetry(
    const std::string_view raw,
    const std::string_view format,
    const std::size_t max_atoms) {
  SymmetryCalculation result;
  const auto parsed = parse_symmetry_atoms_text(raw, format);
  result.parse_error = parsed.error;
  result.atom_count = parsed.atoms.size();
  if (parsed.error != KERNEL_SYMMETRY_PARSE_ERROR_NONE) {
    result.error = KERNEL_SYMMETRY_CALC_ERROR_PARSE;
    return result;
  }
  if (parsed.atoms.empty()) {
    result.error = KERNEL_SYMMETRY_CALC_ERROR_NO_ATOMS;
    return result;
  }
  if (parsed.atoms.size() > max_atoms) {
    result.error = KERNEL_SYMMETRY_CALC_ERROR_TOO_MANY_ATOMS;
    return result;
  }

  if (parsed.atoms.size() == 1) {
    result.point_group = "K_h";
    result.has_inversion = true;
    return result;
  }

  auto atom_inputs = to_atom_inputs(parsed.atoms);
  const auto shape = analyze_symmetry_shape(atom_inputs.data(), atom_inputs.size());
  center_atoms(atom_inputs, shape.center_of_mass);
  result.has_inversion = shape.has_inversion != 0;

  if (shape.is_linear != 0) {
    result.point_group = result.has_inversion ? d_infinity_h() : c_infinity_v();
    kernel_symmetry_axis_input linear_axis{};
    linear_axis.dir[0] = shape.linear_axis[0];
    linear_axis.dir[1] = shape.linear_axis[1];
    linear_axis.dir[2] = shape.linear_axis[2];
    linear_axis.order = 0;
    result.axes.resize(1);
    build_symmetry_render_geometry(
        &linear_axis,
        1,
        nullptr,
        0,
        shape.mol_radius,
        result.axes.data(),
        nullptr);
    return result;
  }

  kernel_symmetry_direction_input principal_axes[3]{};
  compute_symmetry_principal_axes(atom_inputs.data(), atom_inputs.size(), principal_axes);

  std::vector<kernel_symmetry_direction_input> candidate_directions(
      direction_candidate_capacity(atom_inputs.size()));
  const auto direction_result = generate_symmetry_candidate_directions(
      atom_inputs.data(),
      atom_inputs.size(),
      principal_axes,
      3,
      candidate_directions.data(),
      candidate_directions.size());
  if (direction_result.capacity_exceeded) {
    return internal_error();
  }
  candidate_directions.resize(direction_result.count);

  std::vector<kernel_symmetry_axis_input> found_axes(candidate_directions.size() * 5);
  const auto axis_result = find_symmetry_rotation_axes(
      atom_inputs.data(),
      atom_inputs.size(),
      candidate_directions.data(),
      candidate_directions.size(),
      found_axes.data(),
      found_axes.size());
  if (axis_result.capacity_exceeded) {
    return internal_error();
  }
  found_axes.resize(axis_result.count);

  std::vector<kernel_symmetry_plane_input> candidate_planes(
      plane_candidate_capacity(atom_inputs.size(), found_axes.size()));
  const auto plane_candidate_result = generate_symmetry_candidate_planes(
      atom_inputs.data(),
      atom_inputs.size(),
      found_axes.data(),
      found_axes.size(),
      principal_axes,
      3,
      candidate_planes.data(),
      candidate_planes.size());
  if (plane_candidate_result.capacity_exceeded) {
    return internal_error();
  }
  candidate_planes.resize(plane_candidate_result.count);

  std::vector<kernel_symmetry_plane_input> found_planes(candidate_planes.size());
  const auto plane_result = find_symmetry_mirror_planes(
      atom_inputs.data(),
      atom_inputs.size(),
      candidate_planes.data(),
      candidate_planes.size(),
      found_planes.data(),
      found_planes.size());
  if (plane_result.capacity_exceeded) {
    return internal_error();
  }
  found_planes.resize(plane_result.count);

  result.point_group = classify_point_group(
      found_axes.data(),
      found_axes.size(),
      found_planes.data(),
      found_planes.size(),
      result.has_inversion);
  result.axes.resize(found_axes.size());
  result.planes.resize(found_planes.size());
  build_symmetry_render_geometry(
      found_axes.data(),
      found_axes.size(),
      found_planes.data(),
      found_planes.size(),
      shape.mol_radius,
      result.axes.data(),
      result.planes.data());
  return result;
}

}  // namespace kernel::symmetry
