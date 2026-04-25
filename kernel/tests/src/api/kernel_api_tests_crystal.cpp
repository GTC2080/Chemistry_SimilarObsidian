// Reason: Keep stateless crystal compute coverage at the kernel C ABI boundary
// so hosts can stay thin bridges over Miller-plane math.

#include "kernel/c_api.h"

#include "api/kernel_api_test_suites.h"
#include "support/test_support.h"

#include <cmath>
#include <string>
#include <string_view>

namespace {

kernel_crystal_cell_params cubic_cell(const double a) {
  kernel_crystal_cell_params cell{};
  cell.a = a;
  cell.b = a;
  cell.c = a;
  cell.alpha_deg = 90.0;
  cell.beta_deg = 90.0;
  cell.gamma_deg = 90.0;
  return cell;
}

kernel_symmetry_operation_input identity_symop() {
  kernel_symmetry_operation_input op{};
  op.rot[0][0] = 1.0;
  op.rot[1][1] = 1.0;
  op.rot[2][2] = 1.0;
  return op;
}

void require_ok_status(const kernel_status status, std::string_view context) {
  require_true(
      status.code == KERNEL_OK,
      std::string(context) + ": expected KERNEL_OK, got status " +
          std::to_string(status.code));
}

void require_near(const double actual, const double expected, std::string_view context) {
  require_true(
      std::abs(actual - expected) < 1.0e-9,
      std::string(context) + ": expected " + std::to_string(expected) + ", got " +
          std::to_string(actual));
}

void test_cif_parser_extracts_cell_atoms_and_symops() {
  constexpr std::string_view cif = R"cif(
data_test
_cell_length_a 5.0
_cell_length_b 6.0
_cell_length_c 7.0
_cell_angle_alpha 90
_cell_angle_beta 91
_cell_angle_gamma 92

loop_
_space_group_symop_operation_xyz
x,y,z
'-x+1/2,y,-z+1/2'

loop_
_atom_site_type_symbol
_atom_site_fract_x
_atom_site_fract_y
_atom_site_fract_z
C1 0.1(2) 0.2 0.3
Cl 0.5 0.6 0.7
)cif";

  kernel_crystal_parse_result result{};
  require_ok_status(
      kernel_parse_cif_crystal(cif.data(), cif.size(), &result),
      "parse CIF cell atoms symops");

  require_near(result.cell.a, 5.0, "cif cell a");
  require_near(result.cell.gamma_deg, 92.0, "cif cell gamma");
  require_true(result.atom_count == 2, "cif should parse two atoms");
  require_true(result.symop_count == 2, "cif should parse two symmetry operations");
  require_true(
      result.atoms[0].element != nullptr && std::string(result.atoms[0].element) == "C",
      "cif parser should normalize atom labels");
  require_near(result.atoms[1].frac[2], 0.7, "cif atom frac z");
  require_near(result.symops[1].rot[0][0], -1.0, "cif symop signed x");
  require_near(result.symops[1].trans[0], 0.5, "cif symop translation x");
  require_near(result.symops[1].rot[2][2], -1.0, "cif symop signed z");
  require_near(result.symops[1].trans[2], 0.5, "cif symop translation z");

  kernel_free_crystal_parse_result(&result);
  require_true(
      result.atoms == nullptr && result.atom_count == 0 && result.symops == nullptr &&
          result.symop_count == 0,
      "cif parse free should reset arrays");
}

void test_cif_parser_supports_next_line_cell_values_and_default_identity() {
  constexpr std::string_view cif = R"cif(
data_test
_cell_length_a
5.0
_cell_length_b
5.0
_cell_length_c
5.0
_cell_angle_alpha
90
_cell_angle_beta
90
_cell_angle_gamma
90

loop_
_atom_site_label
_atom_site_fract_x
_atom_site_fract_y
_atom_site_fract_z
Na1 0.0 0.0 0.0
)cif";

  kernel_crystal_parse_result result{};
  require_ok_status(
      kernel_parse_cif_crystal(cif.data(), cif.size(), &result),
      "parse CIF next-line cell values");

  require_true(result.atom_count == 1, "cif next-line should parse one atom");
  require_true(result.symop_count == 1, "cif should default to identity symop");
  require_near(result.symops[0].rot[0][0], 1.0, "default symop identity x");
  require_near(result.symops[0].rot[1][1], 1.0, "default symop identity y");
  require_near(result.symops[0].rot[2][2], 1.0, "default symop identity z");

  kernel_free_crystal_parse_result(&result);
}

void test_cif_parser_rejects_missing_contract_parts() {
  constexpr std::string_view no_cell = R"cif(
data_test
loop_
_atom_site_type_symbol
_atom_site_fract_x
_atom_site_fract_y
_atom_site_fract_z
Na 0.0 0.0 0.0
)cif";
  constexpr std::string_view no_atoms = R"cif(
data_test
_cell_length_a 5.0
_cell_length_b 5.0
_cell_length_c 5.0
_cell_angle_alpha 90
_cell_angle_beta 90
_cell_angle_gamma 90
)cif";

  kernel_crystal_parse_result result{};
  require_true(
      kernel_parse_cif_crystal(no_cell.data(), no_cell.size(), &result).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "cif parser should reject missing cell");
  require_true(
      result.error == KERNEL_CRYSTAL_PARSE_ERROR_MISSING_CELL,
      "cif parser should report missing cell");

  require_true(
      kernel_parse_cif_crystal(no_atoms.data(), no_atoms.size(), &result).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "cif parser should reject missing atoms");
  require_true(
      result.error == KERNEL_CRYSTAL_PARSE_ERROR_MISSING_ATOMS,
      "cif parser should report missing atoms");
  require_true(
      kernel_parse_cif_crystal(nullptr, 0, &result).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "cif parser should reject null input");
  require_true(
      kernel_parse_cif_crystal(no_atoms.data(), no_atoms.size(), nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "cif parser should reject null output");
}

void test_miller_plane_100_for_cubic_cell() {
  const auto cell = cubic_cell(5.0);
  kernel_miller_plane_result result{};

  require_ok_status(
      kernel_calculate_miller_plane(&cell, 1, 0, 0, &result),
      "miller plane 100");

  require_near(result.normal[0], 1.0, "miller 100 normal x");
  require_near(result.normal[1], 0.0, "miller 100 normal y");
  require_near(result.normal[2], 0.0, "miller 100 normal z");
  require_near(result.center[0], 5.0, "miller 100 center x");
  require_near(result.center[1], 0.0, "miller 100 center y");
  require_near(result.center[2], 0.0, "miller 100 center z");
  require_near(result.d, -5.0, "miller 100 plane d");
  require_true(result.error == KERNEL_CRYSTAL_MILLER_ERROR_NONE, "miller 100 should clear error");
}

void test_miller_plane_111_for_cubic_cell() {
  const auto cell = cubic_cell(5.0);
  kernel_miller_plane_result result{};

  require_ok_status(
      kernel_calculate_miller_plane(&cell, 1, 1, 1, &result),
      "miller plane 111");

  const double inv_sqrt3 = 1.0 / std::sqrt(3.0);
  require_near(result.normal[0], inv_sqrt3, "miller 111 normal x");
  require_near(result.normal[1], inv_sqrt3, "miller 111 normal y");
  require_near(result.normal[2], inv_sqrt3, "miller 111 normal z");
  require_true(result.vertices[0][0] != result.vertices[2][0], "miller 111 vertices should span");
}

void test_miller_plane_rejects_invalid_inputs_with_typed_errors() {
  auto cell = cubic_cell(5.0);
  kernel_miller_plane_result result{};

  require_true(
      kernel_calculate_miller_plane(&cell, 0, 0, 0, &result).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "miller should reject zero index");
  require_true(
      result.error == KERNEL_CRYSTAL_MILLER_ERROR_ZERO_INDEX,
      "miller should report zero index");

  cell.gamma_deg = 0.0;
  require_true(
      kernel_calculate_miller_plane(&cell, 1, 0, 0, &result).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "miller should reject degenerate gamma");
  require_true(
      result.error == KERNEL_CRYSTAL_MILLER_ERROR_GAMMA_TOO_SMALL,
      "miller should report degenerate gamma");

  require_true(
      kernel_calculate_miller_plane(nullptr, 1, 0, 0, &result).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "miller should reject null cell");
  require_true(
      kernel_calculate_miller_plane(&cell, 1, 0, 0, nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "miller should reject null output");
}

void test_supercell_expands_simple_cubic_cell() {
  const auto cell = cubic_cell(3.0);
  kernel_fractional_atom_input atom{};
  atom.element = "Na";
  atom.frac[0] = 0.0;
  atom.frac[1] = 0.0;
  atom.frac[2] = 0.0;
  const auto symop = identity_symop();
  kernel_supercell_result result{};

  require_ok_status(
      kernel_build_supercell(&cell, &atom, 1, &symop, 1, 2, 2, 2, &result),
      "supercell simple cubic");

  require_true(result.count == 8, "supercell should expand to eight atoms");
  require_true(result.atoms != nullptr, "supercell should allocate atoms");
  require_true(
      result.atoms[0].element != nullptr && std::string(result.atoms[0].element) == "Na",
      "supercell should preserve element");
  require_near(result.atoms[7].cartesian_coords[0], 3.0, "supercell last x");
  require_near(result.atoms[7].cartesian_coords[1], 3.0, "supercell last y");
  require_near(result.atoms[7].cartesian_coords[2], 3.0, "supercell last z");

  kernel_free_supercell_result(&result);
  require_true(
      result.atoms == nullptr && result.count == 0,
      "supercell free should reset result");
}

void test_supercell_deduplicates_identical_symmetry_ops() {
  const auto cell = cubic_cell(3.0);
  kernel_fractional_atom_input atom{};
  atom.element = "Fe";
  atom.frac[0] = 0.0;
  atom.frac[1] = 0.0;
  atom.frac[2] = 0.0;
  const kernel_symmetry_operation_input symops[2] = {identity_symop(), identity_symop()};
  kernel_supercell_result result{};

  require_ok_status(
      kernel_build_supercell(&cell, &atom, 1, symops, 2, 1, 1, 1, &result),
      "supercell dedup identical symops");

  require_true(result.count == 1, "supercell should deduplicate identical symops");
  require_true(
      result.atoms[0].element != nullptr && std::string(result.atoms[0].element) == "Fe",
      "supercell dedup should preserve element");
  kernel_free_supercell_result(&result);
}

void test_supercell_rejects_invalid_inputs_with_typed_errors() {
  auto cell = cubic_cell(3.0);
  kernel_fractional_atom_input atom{};
  atom.element = "Na";
  const auto symop = identity_symop();
  kernel_supercell_result result{};

  require_true(
      kernel_build_supercell(&cell, &atom, 1, &symop, 1, 50001, 1, 1, &result).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "supercell should reject too many atoms");
  require_true(
      result.error == KERNEL_CRYSTAL_SUPERCELL_ERROR_TOO_MANY_ATOMS,
      "supercell should report too many atoms");
  require_true(result.estimated_count == 50001, "supercell should report estimated count");

  cell.gamma_deg = 0.0;
  require_true(
      kernel_build_supercell(&cell, &atom, 1, &symop, 1, 1, 1, 1, &result).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "supercell should reject degenerate gamma");
  require_true(
      result.error == KERNEL_CRYSTAL_SUPERCELL_ERROR_GAMMA_TOO_SMALL,
      "supercell should report degenerate gamma");

  require_true(
      kernel_build_supercell(nullptr, &atom, 1, &symop, 1, 1, 1, 1, &result).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "supercell should reject null cell");
  require_true(
      kernel_build_supercell(&cell, nullptr, 1, &symop, 1, 1, 1, 1, &result).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "supercell should reject null atoms when count is nonzero");
  require_true(
      kernel_build_supercell(&cell, &atom, 1, nullptr, 1, 1, 1, 1, &result).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "supercell should reject null symops when count is nonzero");
  require_true(
      kernel_build_supercell(&cell, &atom, 1, &symop, 1, 1, 1, 1, nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "supercell should reject null output");
}

void test_lattice_from_cif_builds_full_viewer_payload() {
  constexpr std::string_view cif = R"cif(
data_NaCl
_cell_length_a 5.64
_cell_length_b 5.64
_cell_length_c 5.64
_cell_angle_alpha 90
_cell_angle_beta 90
_cell_angle_gamma 90

loop_
_symmetry_equiv_pos_as_xyz
x,y,z

loop_
_atom_site_type_symbol
_atom_site_fract_x
_atom_site_fract_y
_atom_site_fract_z
Na 0.0 0.0 0.0
Cl 0.5 0.5 0.5
)cif";

  kernel_lattice_result result{};
  require_ok_status(
      kernel_build_lattice_from_cif(cif.data(), cif.size(), 2, 2, 2, &result),
      "full lattice from CIF");

  require_near(result.unit_cell.a, 5.64, "lattice unit cell a");
  require_near(result.unit_cell.vectors[0][0], 5.64, "lattice vector ax");
  require_true(result.atom_count == 16, "lattice should emit expanded atom payload");
  require_true(
      result.atoms != nullptr && result.atoms[0].element != nullptr,
      "lattice should allocate atom payload");
  require_true(result.parse_error == KERNEL_CRYSTAL_PARSE_ERROR_NONE, "lattice parse ok");
  require_true(
      result.supercell_error == KERNEL_CRYSTAL_SUPERCELL_ERROR_NONE,
      "lattice supercell ok");

  kernel_free_lattice_result(&result);
  require_true(result.atoms == nullptr && result.atom_count == 0, "lattice free resets atoms");
}

void test_lattice_from_cif_reports_typed_parse_and_supercell_errors() {
  constexpr std::string_view no_cell = R"cif(
data_test
loop_
_atom_site_type_symbol
_atom_site_fract_x
_atom_site_fract_y
_atom_site_fract_z
Na 0.0 0.0 0.0
)cif";
  constexpr std::string_view too_many = R"cif(
data_test
_cell_length_a 3.0
_cell_length_b 3.0
_cell_length_c 3.0
_cell_angle_alpha 90
_cell_angle_beta 90
_cell_angle_gamma 90

loop_
_atom_site_type_symbol
_atom_site_fract_x
_atom_site_fract_y
_atom_site_fract_z
Na 0.0 0.0 0.0
)cif";

  kernel_lattice_result result{};
  require_true(
      kernel_build_lattice_from_cif(no_cell.data(), no_cell.size(), 1, 1, 1, &result).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "full lattice should reject missing cell");
  require_true(
      result.parse_error == KERNEL_CRYSTAL_PARSE_ERROR_MISSING_CELL,
      "full lattice should report parse error");

  require_true(
      kernel_build_lattice_from_cif(too_many.data(), too_many.size(), 50001, 1, 1, &result)
              .code == KERNEL_ERROR_INVALID_ARGUMENT,
      "full lattice should reject too many atoms");
  require_true(
      result.supercell_error == KERNEL_CRYSTAL_SUPERCELL_ERROR_TOO_MANY_ATOMS,
      "full lattice should report supercell error");
  require_true(result.estimated_count == 50001, "full lattice should report estimate");
}

void test_miller_plane_from_cif_keeps_host_bridge_thin() {
  constexpr std::string_view cif = R"cif(
data_test
_cell_length_a 5.0
_cell_length_b 5.0
_cell_length_c 5.0
_cell_angle_alpha 90
_cell_angle_beta 90
_cell_angle_gamma 90

loop_
_atom_site_type_symbol
_atom_site_fract_x
_atom_site_fract_y
_atom_site_fract_z
Na 0.0 0.0 0.0
)cif";

  kernel_cif_miller_plane_result result{};
  require_ok_status(
      kernel_calculate_miller_plane_from_cif(cif.data(), cif.size(), 1, 1, 1, &result),
      "miller plane from CIF");

  const double inv_sqrt3 = 1.0 / std::sqrt(3.0);
  require_near(result.plane.normal[0], inv_sqrt3, "cif miller normal x");
  require_true(result.parse_error == KERNEL_CRYSTAL_PARSE_ERROR_NONE, "cif miller parse ok");
  require_true(
      result.plane.error == KERNEL_CRYSTAL_MILLER_ERROR_NONE,
      "cif miller plane ok");

  require_true(
      kernel_calculate_miller_plane_from_cif(cif.data(), cif.size(), 0, 0, 0, &result).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "cif miller should reject zero index");
  require_true(
      result.plane.error == KERNEL_CRYSTAL_MILLER_ERROR_ZERO_INDEX,
      "cif miller should report zero index");
}

}  // namespace

void run_crystal_compute_tests() {
  test_cif_parser_extracts_cell_atoms_and_symops();
  test_cif_parser_supports_next_line_cell_values_and_default_identity();
  test_cif_parser_rejects_missing_contract_parts();
  test_miller_plane_100_for_cubic_cell();
  test_miller_plane_111_for_cubic_cell();
  test_miller_plane_rejects_invalid_inputs_with_typed_errors();
  test_supercell_expands_simple_cubic_cell();
  test_supercell_deduplicates_identical_symmetry_ops();
  test_supercell_rejects_invalid_inputs_with_typed_errors();
  test_lattice_from_cif_builds_full_viewer_payload();
  test_lattice_from_cif_reports_typed_parse_and_supercell_errors();
  test_miller_plane_from_cif_keeps_host_bridge_thin();
}
