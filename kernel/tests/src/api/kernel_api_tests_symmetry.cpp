// Reason: Keep point-group classification coverage at the kernel C ABI
// boundary so hosts can stay thin bridges over symmetry rules.

#include "kernel/c_api.h"

#include "api/kernel_api_test_suites.h"
#include "support/test_support.h"

#include <cmath>
#include <string>
#include <string_view>

namespace {

void require_ok_status(const kernel_status status, std::string_view context) {
  require_true(
      status.code == KERNEL_OK,
      std::string(context) + ": expected KERNEL_OK, got status " +
          std::to_string(status.code));
}

kernel_symmetry_axis_input axis(const double x, const double y, const double z, const uint8_t order) {
  kernel_symmetry_axis_input result{};
  result.dir[0] = x;
  result.dir[1] = y;
  result.dir[2] = z;
  result.order = order;
  return result;
}

kernel_symmetry_plane_input plane(const double x, const double y, const double z) {
  kernel_symmetry_plane_input result{};
  result.normal[0] = x;
  result.normal[1] = y;
  result.normal[2] = z;
  return result;
}

kernel_symmetry_direction_input direction(const double x, const double y, const double z) {
  kernel_symmetry_direction_input result{};
  result.dir[0] = x;
  result.dir[1] = y;
  result.dir[2] = z;
  return result;
}

kernel_symmetry_atom_input atom_input(
    const char* element,
    const double x,
    const double y,
    const double z,
    const double mass) {
  kernel_symmetry_atom_input result{};
  result.element = element;
  result.position[0] = x;
  result.position[1] = y;
  result.position[2] = z;
  result.mass = mass;
  return result;
}

void require_near(const double actual, const double expected, std::string_view message) {
  require_true(
      std::abs(actual - expected) < 1.0e-8,
      std::string(message) + ": expected " + std::to_string(expected) + ", got " +
          std::to_string(actual));
}

bool has_parallel_direction(
    const kernel_symmetry_direction_input* directions,
    const size_t count,
    const double x,
    const double y,
    const double z) {
  const double target_len = std::sqrt(x * x + y * y + z * z);
  if (target_len < 1.0e-12) {
    return false;
  }
  const double target[3] = {x / target_len, y / target_len, z / target_len};
  for (size_t index = 0; index < count; ++index) {
    const double dot =
        directions[index].dir[0] * target[0] + directions[index].dir[1] * target[1] +
        directions[index].dir[2] * target[2];
    if (std::abs(dot) > std::cos(0.10)) {
      return true;
    }
  }
  return false;
}

bool has_parallel_plane(
    const kernel_symmetry_plane_input* planes,
    const size_t count,
    const double x,
    const double y,
    const double z) {
  const double target_len = std::sqrt(x * x + y * y + z * z);
  if (target_len < 1.0e-12) {
    return false;
  }
  const double target[3] = {x / target_len, y / target_len, z / target_len};
  for (size_t index = 0; index < count; ++index) {
    const double dot =
        planes[index].normal[0] * target[0] + planes[index].normal[1] * target[1] +
        planes[index].normal[2] * target[2];
    if (std::abs(dot) > std::cos(0.10)) {
      return true;
    }
  }
  return false;
}

std::string classify(
    const kernel_symmetry_axis_input* axes,
    const size_t axis_count,
    const kernel_symmetry_plane_input* planes,
    const size_t plane_count,
    const bool has_inversion) {
  kernel_symmetry_classification_result result{};
  require_ok_status(
      kernel_classify_point_group(
          axes,
          axis_count,
          planes,
          plane_count,
          has_inversion ? 1 : 0,
          &result),
      "classify point group");
  return result.point_group;
}

void test_symmetry_classifies_c2v() {
  const kernel_symmetry_axis_input axes[] = {axis(0.0, 0.0, 1.0, 2)};
  const kernel_symmetry_plane_input planes[] = {
      plane(1.0, 0.0, 0.0),
      plane(0.0, 1.0, 0.0),
  };

  require_true(
      classify(axes, 1, planes, 2, false) == "C_2v",
      "symmetry classifier should classify C2v");
}

void test_symmetry_classifies_d2h() {
  const kernel_symmetry_axis_input axes[] = {
      axis(0.0, 0.0, 1.0, 2),
      axis(1.0, 0.0, 0.0, 2),
      axis(0.0, 1.0, 0.0, 2),
  };
  const kernel_symmetry_plane_input planes[] = {plane(0.0, 0.0, 1.0)};

  require_true(
      classify(axes, 3, planes, 1, true) == "D_2h",
      "symmetry classifier should classify D2h");
}

void test_symmetry_classifies_low_symmetry_cases() {
  const kernel_symmetry_plane_input mirror[] = {plane(1.0, 0.0, 0.0)};

  require_true(classify(nullptr, 0, nullptr, 0, false) == "C_1", "empty symmetry should be C1");
  require_true(classify(nullptr, 0, mirror, 1, false) == "C_s", "mirror only should be Cs");
  require_true(classify(nullptr, 0, nullptr, 0, true) == "C_i", "inversion only should be Ci");
}

void test_symmetry_rejects_invalid_inputs() {
  kernel_symmetry_classification_result result{};
  const auto valid_axis = axis(0.0, 0.0, 1.0, 2);
  const auto valid_plane = plane(1.0, 0.0, 0.0);

  require_true(
      kernel_classify_point_group(&valid_axis, 1, &valid_plane, 1, 0, nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "symmetry classifier should reject null output");
  require_true(
      kernel_classify_point_group(nullptr, 1, &valid_plane, 1, 0, &result).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "symmetry classifier should reject null axes when count is nonzero");
  require_true(
      kernel_classify_point_group(&valid_axis, 1, nullptr, 1, 0, &result).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "symmetry classifier should reject null planes when count is nonzero");
}

void test_symmetry_analyzes_linear_shape() {
  const kernel_symmetry_atom_input atoms[] = {
      atom_input("C", 0.0, 0.0, 0.0, 12.0),
      atom_input("O", 0.0, 0.0, 1.16, 16.0),
      atom_input("O", 0.0, 0.0, -1.16, 16.0),
  };
  kernel_symmetry_shape_result result{};

  require_ok_status(kernel_analyze_symmetry_shape(atoms, 3, &result), "analyze linear symmetry shape");

  require_near(result.center_of_mass[2], 0.0, "linear shape center z");
  require_near(result.mol_radius, 1.16, "linear shape radius");
  require_true(result.is_linear == 1, "CO2 shape should be linear");
  require_near(result.linear_axis[2], 1.0, "linear shape axis z");
  require_true(result.has_inversion == 1, "CO2 shape should have inversion");
}

void test_symmetry_analyzes_nonlinear_shape() {
  const kernel_symmetry_atom_input atoms[] = {
      atom_input("O", 0.0, 0.0, 0.117, 16.0),
      atom_input("H", 0.0, 0.757, -0.469, 1.0),
      atom_input("H", 0.0, -0.757, -0.469, 1.0),
  };
  kernel_symmetry_shape_result result{};

  require_ok_status(
      kernel_analyze_symmetry_shape(atoms, 3, &result),
      "analyze nonlinear symmetry shape");

  require_true(result.is_linear == 0, "water shape should be nonlinear");
  require_true(result.has_inversion == 0, "water shape should not have inversion");
  require_true(result.mol_radius >= 1.0, "shape radius should keep viewer minimum");
}

void test_symmetry_shape_rejects_invalid_inputs() {
  const auto valid_atom = atom_input("He", 0.0, 0.0, 0.0, 4.0);
  const auto null_element = atom_input(nullptr, 0.0, 0.0, 0.0, 4.0);
  kernel_symmetry_shape_result result{};

  require_true(
      kernel_analyze_symmetry_shape(nullptr, 1, &result).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "symmetry shape should reject null atoms");
  require_true(
      kernel_analyze_symmetry_shape(&valid_atom, 0, &result).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "symmetry shape should reject zero atoms");
  require_true(
      kernel_analyze_symmetry_shape(&valid_atom, 1, nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "symmetry shape should reject null output");
  require_true(
      kernel_analyze_symmetry_shape(&null_element, 1, &result).code ==
      KERNEL_ERROR_INVALID_ARGUMENT,
      "symmetry shape should reject null element");
}

void test_symmetry_computes_principal_axes() {
  const kernel_symmetry_atom_input atoms[] = {
      atom_input("H", 1.0, 1.0, 0.0, 1.0),
      atom_input("H", -1.0, -1.0, 0.0, 1.0),
  };
  kernel_symmetry_direction_input axes[3]{};

  require_ok_status(
      kernel_compute_symmetry_principal_axes(atoms, 2, axes),
      "compute symmetry principal axes");

  require_true(
      has_parallel_direction(axes, 3, 1.0, 1.0, 0.0),
      "principal axes should include the molecular line");
  for (const auto& axis_value : axes) {
    const double len = std::sqrt(
        axis_value.dir[0] * axis_value.dir[0] +
        axis_value.dir[1] * axis_value.dir[1] +
        axis_value.dir[2] * axis_value.dir[2]);
    require_near(len, 1.0, "principal axis length");
  }
}

void test_symmetry_principal_axes_rejects_invalid_inputs() {
  const auto valid_atom = atom_input("He", 0.0, 0.0, 0.0, 4.0);
  const auto null_element = atom_input(nullptr, 0.0, 0.0, 0.0, 4.0);
  kernel_symmetry_direction_input axes[3] = {
      direction(9.0, 9.0, 9.0),
      direction(9.0, 9.0, 9.0),
      direction(9.0, 9.0, 9.0),
  };

  require_true(
      kernel_compute_symmetry_principal_axes(nullptr, 1, axes).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "principal-axis calculation should reject null atoms");
  require_near(axes[0].dir[0], 0.0, "principal-axis invalid reset");
  require_true(
      kernel_compute_symmetry_principal_axes(&valid_atom, 0, axes).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "principal-axis calculation should reject zero atoms");
  require_true(
      kernel_compute_symmetry_principal_axes(&valid_atom, 1, nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "principal-axis calculation should reject null output");
  require_true(
      kernel_compute_symmetry_principal_axes(&null_element, 1, axes).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "principal-axis calculation should reject null atom elements");
}

void test_symmetry_generates_candidate_directions() {
  const kernel_symmetry_atom_input atoms[] = {
      atom_input("O", 0.0, 0.0, 0.0, 16.0),
      atom_input("H", 0.0, 0.757, -0.586, 1.0),
      atom_input("H", 0.0, -0.757, -0.586, 1.0),
  };
  const kernel_symmetry_direction_input principal_axes[] = {
      direction(1.0, 0.0, 0.0),
      direction(0.0, 1.0, 0.0),
      direction(0.0, 0.0, 1.0),
  };
  kernel_symmetry_direction_input out_directions[32]{};
  size_t out_count = 0;

  require_ok_status(
      kernel_generate_symmetry_candidate_directions(
          atoms,
          3,
          principal_axes,
          3,
          out_directions,
          32,
          &out_count),
      "generate symmetry candidate directions");

  require_true(out_count >= 3, "candidate directions should include base axes");
  require_true(has_parallel_direction(out_directions, out_count, 1.0, 0.0, 0.0), "candidate x");
  require_true(has_parallel_direction(out_directions, out_count, 0.0, 1.0, 0.0), "candidate y");
  require_true(has_parallel_direction(out_directions, out_count, 0.0, 0.0, 1.0), "candidate z");
}

void test_symmetry_generates_candidate_planes() {
  const kernel_symmetry_atom_input atoms[] = {
      atom_input("O", 0.0, 0.0, 0.0, 16.0),
      atom_input("H", 0.0, 0.757, -0.586, 1.0),
      atom_input("H", 0.0, -0.757, -0.586, 1.0),
  };
  const kernel_symmetry_axis_input axes[] = {axis(0.0, 0.0, 1.0, 2)};
  const kernel_symmetry_direction_input principal_axes[] = {
      direction(1.0, 0.0, 0.0),
      direction(0.0, 1.0, 0.0),
      direction(0.0, 0.0, 1.0),
  };
  kernel_symmetry_plane_input out_planes[32]{};
  size_t out_count = 0;

  require_ok_status(
      kernel_generate_symmetry_candidate_planes(
          atoms,
          3,
          axes,
          1,
          principal_axes,
          3,
          out_planes,
          32,
          &out_count),
      "generate symmetry candidate planes");

  require_true(out_count >= 3, "candidate planes should include base normals");
  require_true(has_parallel_plane(out_planes, out_count, 1.0, 0.0, 0.0), "candidate plane x");
  require_true(has_parallel_plane(out_planes, out_count, 0.0, 1.0, 0.0), "candidate plane y");
  require_true(has_parallel_plane(out_planes, out_count, 0.0, 0.0, 1.0), "candidate plane z");
}

void test_symmetry_candidate_generation_rejects_invalid_inputs() {
  const auto valid_atom = atom_input("He", 0.0, 0.0, 0.0, 4.0);
  const auto valid_axis = axis(0.0, 0.0, 1.0, 2);
  const auto valid_direction = direction(0.0, 0.0, 1.0);
  kernel_symmetry_direction_input out_direction{};
  kernel_symmetry_plane_input out_plane{};
  size_t out_count = 99;

  require_true(
      kernel_generate_symmetry_candidate_directions(
          nullptr,
          1,
          &valid_direction,
          1,
          &out_direction,
          1,
          &out_count)
              .code == KERNEL_ERROR_INVALID_ARGUMENT,
      "candidate direction generation should reject null atoms");
  require_true(out_count == 0, "candidate direction generation should reset invalid count");
  require_true(
      kernel_generate_symmetry_candidate_directions(
          &valid_atom,
          1,
          &valid_direction,
          1,
          &out_direction,
          1,
          nullptr)
              .code == KERNEL_ERROR_INVALID_ARGUMENT,
      "candidate direction generation should reject null count output");
  require_true(
      kernel_generate_symmetry_candidate_planes(
          &valid_atom,
          1,
          &valid_axis,
          1,
          &valid_direction,
          1,
          nullptr,
          1,
          &out_count)
              .code == KERNEL_ERROR_INVALID_ARGUMENT,
      "candidate plane generation should reject missing output when capacity is nonzero");
  require_true(
      kernel_generate_symmetry_candidate_planes(
          &valid_atom,
          1,
          &valid_axis,
          1,
          &valid_direction,
          1,
          &out_plane,
          0,
          &out_count)
              .code == KERNEL_ERROR_INVALID_ARGUMENT,
      "candidate plane generation should reject too-small capacity");
}

void test_symmetry_finds_rotation_axes() {
  const kernel_symmetry_atom_input atoms[] = {
      atom_input("O", 0.0, 0.0, 0.0, 16.0),
      atom_input("H", 0.0, 0.757, -0.586, 1.0),
      atom_input("H", 0.0, -0.757, -0.586, 1.0),
  };
  const kernel_symmetry_direction_input candidates[] = {
      direction(0.0, 0.0, 1.0),
      direction(1.0, 0.0, 0.0),
  };
  kernel_symmetry_axis_input out_axes[10]{};
  size_t out_count = 0;

  require_ok_status(
      kernel_find_symmetry_rotation_axes(atoms, 3, candidates, 2, out_axes, 10, &out_count),
      "find symmetry rotation axes");

  require_true(out_count == 1, "water operation search should find one C2 axis");
  require_true(out_axes[0].order == 2, "water operation search should emit C2 order");
  require_near(out_axes[0].dir[2], 1.0, "water C2 axis z");
}

void test_symmetry_finds_mirror_planes() {
  const kernel_symmetry_atom_input atoms[] = {
      atom_input("O", 0.0, 0.0, 0.0, 16.0),
      atom_input("H", 0.0, 0.757, -0.586, 1.0),
      atom_input("H", 0.0, -0.757, -0.586, 1.0),
  };
  const kernel_symmetry_plane_input candidates[] = {
      plane(1.0, 0.0, 0.0),
      plane(0.0, 1.0, 0.0),
      plane(0.0, 0.0, 1.0),
  };
  kernel_symmetry_plane_input out_planes[3]{};
  size_t out_count = 0;

  require_ok_status(
      kernel_find_symmetry_mirror_planes(atoms, 3, candidates, 3, out_planes, 3, &out_count),
      "find symmetry mirror planes");

  require_true(out_count == 2, "water operation search should find two mirror planes");
  require_near(out_planes[0].normal[0], 1.0, "first water mirror normal x");
  require_near(out_planes[1].normal[1], 1.0, "second water mirror normal y");
}

void test_symmetry_operation_search_rejects_invalid_inputs() {
  const auto valid_atom = atom_input("He", 0.0, 0.0, 0.0, 4.0);
  const auto valid_direction = direction(0.0, 0.0, 1.0);
  const auto valid_plane = plane(1.0, 0.0, 0.0);
  kernel_symmetry_axis_input out_axis{};
  kernel_symmetry_plane_input out_plane{};
  size_t out_count = 99;

  require_true(
      kernel_find_symmetry_rotation_axes(
          nullptr,
          1,
          &valid_direction,
          1,
          &out_axis,
          1,
          &out_count)
              .code == KERNEL_ERROR_INVALID_ARGUMENT,
      "rotation search should reject null atoms");
  require_true(out_count == 0, "rotation search should reset count on invalid atoms");
  require_true(
      kernel_find_symmetry_rotation_axes(
          &valid_atom,
          1,
          &valid_direction,
          1,
          &out_axis,
          1,
          nullptr)
              .code == KERNEL_ERROR_INVALID_ARGUMENT,
      "rotation search should reject null count output");
  require_true(
      kernel_find_symmetry_mirror_planes(
          &valid_atom,
          1,
          &valid_plane,
          1,
          nullptr,
          1,
          &out_count)
              .code == KERNEL_ERROR_INVALID_ARGUMENT,
      "mirror search should reject null plane output when capacity is nonzero");
}

void test_symmetry_builds_render_geometry() {
  const kernel_symmetry_axis_input axes[] = {axis(0.0, 0.0, 1.0, 2)};
  const kernel_symmetry_plane_input planes[] = {plane(0.0, 0.0, 1.0)};
  kernel_symmetry_render_axis out_axes[1]{};
  kernel_symmetry_render_plane out_planes[1]{};

  require_ok_status(
      kernel_build_symmetry_render_geometry(axes, 1, planes, 1, 2.0, out_axes, out_planes),
      "build symmetry render geometry");

  require_near(out_axes[0].vector[2], 1.0, "render axis vector z");
  require_true(out_axes[0].order == 2, "render axis should preserve order");
  require_near(out_axes[0].center[0], 0.0, "render axis center x");
  require_near(out_axes[0].start[2], -3.0, "render axis start z");
  require_near(out_axes[0].end[2], 3.0, "render axis end z");

  require_near(out_planes[0].normal[2], 1.0, "render plane normal z");
  require_near(out_planes[0].center[0], 0.0, "render plane center x");
  require_near(out_planes[0].vertices[0][0], -3.6, "render plane first vertex x");
  require_near(out_planes[0].vertices[0][1], 3.6, "render plane first vertex y");
  require_near(out_planes[0].vertices[2][1], -3.6, "render plane opposite vertex y");
}

void test_symmetry_render_geometry_rejects_invalid_inputs() {
  const auto valid_axis = axis(0.0, 0.0, 1.0, 2);
  const auto valid_plane = plane(0.0, 0.0, 1.0);
  kernel_symmetry_render_axis out_axis{};
  kernel_symmetry_render_plane out_plane{};

  const auto null_axis_output = kernel_build_symmetry_render_geometry(
      &valid_axis,
      1,
      &valid_plane,
      1,
      1.0,
      nullptr,
      &out_plane);
  require_true(
      null_axis_output.code == KERNEL_ERROR_INVALID_ARGUMENT,
      "symmetry render geometry should reject null axis output");

  const auto null_plane_input = kernel_build_symmetry_render_geometry(
      &valid_axis,
      1,
      nullptr,
      1,
      1.0,
      &out_axis,
      &out_plane);
  require_true(
      null_plane_input.code == KERNEL_ERROR_INVALID_ARGUMENT,
      "symmetry render geometry should reject null plane input");

  const auto negative_radius = kernel_build_symmetry_render_geometry(
      nullptr,
      0,
      nullptr,
      0,
      -1.0,
      nullptr,
      nullptr);
  require_true(
      negative_radius.code == KERNEL_ERROR_INVALID_ARGUMENT,
      "symmetry render geometry should reject negative radius");
  require_ok_status(
      kernel_build_symmetry_render_geometry(nullptr, 0, nullptr, 0, 1.0, nullptr, nullptr),
      "empty symmetry render geometry");
}

void test_symmetry_parses_xyz_atoms() {
  const std::string xyz =
      "3\n"
      "water\n"
      "O 0.000 0.000 0.117\n"
      "h 0.000 0.757 -0.469\n"
      "FE 0.000 -0.757 -0.469\n";

  kernel_symmetry_atom_list atoms{};
  require_ok_status(
      kernel_parse_symmetry_atoms_text(xyz.data(), xyz.size(), "xyz", &atoms),
      "parse symmetry xyz atoms");

  require_true(atoms.count == 3, "xyz parser should emit three atoms");
  require_true(std::string(atoms.atoms[1].element) == "H", "xyz parser should normalize H");
  require_true(std::string(atoms.atoms[2].element) == "Fe", "xyz parser should normalize Fe");
  require_true(atoms.atoms[2].mass > 55.0, "xyz parser should attach atomic mass");

  kernel_free_symmetry_atom_list(&atoms);
  require_true(
      atoms.atoms == nullptr && atoms.count == 0,
      "symmetry atom free should reset output");
}

void test_symmetry_parses_pdb_atoms() {
  const std::string pdb =
      "ATOM      1  CA  GLY A   1      11.104  13.207   2.120  1.00 20.00           C  \n"
      "HETATM    2  O   HOH A   2      12.104  14.207   3.120  1.00 20.00           O  \n";

  kernel_symmetry_atom_list atoms{};
  require_ok_status(
      kernel_parse_symmetry_atoms_text(pdb.data(), pdb.size(), "pdb", &atoms),
      "parse symmetry pdb atoms");

  require_true(atoms.count == 2, "pdb parser should emit two atoms");
  require_true(std::string(atoms.atoms[0].element) == "C", "pdb parser should use element column");
  require_true(atoms.atoms[1].position[2] > 3.0, "pdb parser should parse z coordinate");

  kernel_free_symmetry_atom_list(&atoms);
}

void test_symmetry_parses_cif_fractional_atoms() {
  const std::string cif =
      "data_demo\n"
      "_cell_length_a\n"
      "3.0\n"
      "_cell_length_b\n"
      "3.0\n"
      "_cell_length_c\n"
      "3.0\n"
      "_cell_angle_alpha\n"
      "90\n"
      "_cell_angle_beta 90\n"
      "_cell_angle_gamma\n"
      "90\n"
      "loop_\n"
      "_atom_site_type_symbol\n"
      "_atom_site_fract_x\n"
      "_atom_site_fract_y\n"
      "_atom_site_fract_z\n"
      "C 0.0 0.0 0.0\n"
      "O 0.5 0.0 0.0\n";

  kernel_symmetry_atom_list atoms{};
  require_ok_status(
      kernel_parse_symmetry_atoms_text(cif.data(), cif.size(), "cif", &atoms),
      "parse symmetry cif atoms");

  require_true(atoms.count == 2, "cif parser should emit two atoms");
  require_true(
      std::abs(atoms.atoms[1].position[0] - 1.5) < 1.0e-8,
      "cif parser should convert fractional x to cartesian");

  kernel_free_symmetry_atom_list(&atoms);
}

void test_symmetry_parser_rejects_invalid_inputs() {
  kernel_symmetry_atom_list atoms{};
  require_true(
      kernel_parse_symmetry_atoms_text("1\n", 2, "xyz", &atoms).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "xyz parser should reject incomplete files");
  require_true(
      atoms.error == KERNEL_SYMMETRY_PARSE_ERROR_XYZ_INCOMPLETE,
      "xyz parser should expose typed incomplete error");
  kernel_free_symmetry_atom_list(&atoms);

  require_true(
      kernel_parse_symmetry_atoms_text("demo", 4, "mol", &atoms).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "symmetry parser should reject unsupported formats");
  require_true(
      atoms.error == KERNEL_SYMMETRY_PARSE_ERROR_UNSUPPORTED_FORMAT,
      "symmetry parser should expose unsupported format error");
  kernel_free_symmetry_atom_list(&atoms);
}

void test_symmetry_calculates_full_water_pipeline() {
  const std::string xyz =
      "3\n"
      "water\n"
      "O  0.000  0.000  0.117\n"
      "H  0.000  0.757 -0.469\n"
      "H  0.000 -0.757 -0.469\n";
  kernel_symmetry_calculation_result result{};

  require_ok_status(
      kernel_calculate_symmetry(xyz.data(), xyz.size(), "xyz", 500, &result),
      "calculate full symmetry water pipeline");

  require_true(std::string(result.point_group) == "C_2v", "water should classify as C2v");
  require_true(result.atom_count == 3, "water atom count");
  require_true(result.axis_count > 0, "water should emit render axes");
  require_true(result.plane_count > 0, "water should emit render planes");
  require_true(result.axes != nullptr, "water axes pointer");
  require_true(result.planes != nullptr, "water planes pointer");
  kernel_free_symmetry_calculation_result(&result);
  require_true(result.axes == nullptr && result.planes == nullptr, "symmetry result free resets arrays");
}

void test_symmetry_calculates_full_linear_pipeline() {
  const std::string xyz =
      "3\n"
      "CO2\n"
      "C  0.000  0.000  0.000\n"
      "O  0.000  0.000  1.160\n"
      "O  0.000  0.000 -1.160\n";
  kernel_symmetry_calculation_result result{};

  require_ok_status(
      kernel_calculate_symmetry(xyz.data(), xyz.size(), "xyz", 500, &result),
      "calculate full symmetry linear pipeline");

  require_true(
      std::string(result.point_group) == (std::string("D") + "\xE2\x88\x9E" + "h"),
      "CO2 should classify as D infinity h");
  require_true(result.has_inversion == 1, "CO2 should have inversion");
  require_true(result.axis_count == 1, "linear molecule should emit one infinite axis");
  require_true(result.plane_count == 0, "linear molecule should not emit render planes");
  kernel_free_symmetry_calculation_result(&result);
}

void test_symmetry_calculation_rejects_invalid_inputs() {
  const std::string invalid_xyz = "1\n";
  kernel_symmetry_calculation_result result{};

  require_true(
      kernel_calculate_symmetry(nullptr, 0, "xyz", 500, &result).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "full symmetry calculation should reject null raw input");
  require_true(
      kernel_calculate_symmetry(invalid_xyz.data(), invalid_xyz.size(), "xyz", 0, &result).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "full symmetry calculation should reject zero max atoms");
  require_true(
      kernel_calculate_symmetry(invalid_xyz.data(), invalid_xyz.size(), "xyz", 500, &result).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "full symmetry calculation should reject parse failures");
  require_true(
      result.error == KERNEL_SYMMETRY_CALC_ERROR_PARSE,
      "full symmetry calculation should expose parse error");
  require_true(
      result.parse_error == KERNEL_SYMMETRY_PARSE_ERROR_XYZ_INCOMPLETE,
      "full symmetry calculation should preserve parser error");
  kernel_free_symmetry_calculation_result(&result);

  const std::string two_atoms =
      "2\n"
      "two\n"
      "H 0 0 0\n"
      "H 0 0 1\n";
  require_true(
      kernel_calculate_symmetry(two_atoms.data(), two_atoms.size(), "xyz", 1, &result).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "full symmetry calculation should reject atom-count limit overflow");
  require_true(
      result.error == KERNEL_SYMMETRY_CALC_ERROR_TOO_MANY_ATOMS,
      "full symmetry calculation should expose too-many-atoms error");
  require_true(result.atom_count == 2, "full symmetry calculation should expose parsed atom count");
  kernel_free_symmetry_calculation_result(&result);
}

}  // namespace

void run_symmetry_compute_tests() {
  test_symmetry_classifies_c2v();
  test_symmetry_classifies_d2h();
  test_symmetry_classifies_low_symmetry_cases();
  test_symmetry_rejects_invalid_inputs();
  test_symmetry_analyzes_linear_shape();
  test_symmetry_analyzes_nonlinear_shape();
  test_symmetry_shape_rejects_invalid_inputs();
  test_symmetry_computes_principal_axes();
  test_symmetry_principal_axes_rejects_invalid_inputs();
  test_symmetry_generates_candidate_directions();
  test_symmetry_generates_candidate_planes();
  test_symmetry_candidate_generation_rejects_invalid_inputs();
  test_symmetry_finds_rotation_axes();
  test_symmetry_finds_mirror_planes();
  test_symmetry_operation_search_rejects_invalid_inputs();
  test_symmetry_builds_render_geometry();
  test_symmetry_render_geometry_rejects_invalid_inputs();
  test_symmetry_parses_xyz_atoms();
  test_symmetry_parses_pdb_atoms();
  test_symmetry_parses_cif_fractional_atoms();
  test_symmetry_parser_rejects_invalid_inputs();
  test_symmetry_calculates_full_water_pipeline();
  test_symmetry_calculates_full_linear_pipeline();
  test_symmetry_calculation_rejects_invalid_inputs();
}
