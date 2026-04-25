// Reason: This file exposes stateless crystal compute helpers through the
// kernel C ABI so Tauri Rust no longer owns Miller-plane math.

#include "kernel/c_api.h"

#include "core/kernel_shared.h"
#include "crystal/cif_parser.h"
#include "crystal/miller_plane.h"
#include "crystal/supercell.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <new>
#include <string_view>
#include <vector>

namespace {

constexpr double kCrystalEpsilon = 1.0e-8;
constexpr double kPi = 3.141592653589793238462643383279502884;

void reset_crystal_parse_result(kernel_crystal_parse_result* result) {
  if (result == nullptr) {
    return;
  }
  if (result->atoms != nullptr) {
    for (std::size_t index = 0; index < result->atom_count; ++index) {
      delete[] result->atoms[index].element;
      result->atoms[index].element = nullptr;
    }
    delete[] result->atoms;
  }
  delete[] result->symops;
  std::memset(&result->cell, 0, sizeof(result->cell));
  result->atoms = nullptr;
  result->atom_count = 0;
  result->symops = nullptr;
  result->symop_count = 0;
  result->error = KERNEL_CRYSTAL_PARSE_ERROR_NONE;
}

void reset_miller_plane_result(kernel_miller_plane_result* result) {
  if (result == nullptr) {
    return;
  }
  std::memset(result, 0, sizeof(kernel_miller_plane_result));
  result->error = KERNEL_CRYSTAL_MILLER_ERROR_NONE;
}

void reset_supercell_result(kernel_supercell_result* result) {
  if (result == nullptr) {
    return;
  }
  if (result->atoms != nullptr) {
    for (std::size_t index = 0; index < result->count; ++index) {
      delete[] result->atoms[index].element;
    }
    delete[] result->atoms;
  }
  result->atoms = nullptr;
  result->count = 0;
  result->estimated_count = 0;
  result->error = KERNEL_CRYSTAL_SUPERCELL_ERROR_NONE;
}

void reset_lattice_result(kernel_lattice_result* result) {
  if (result == nullptr) {
    return;
  }
  if (result->atoms != nullptr) {
    for (std::size_t index = 0; index < result->atom_count; ++index) {
      delete[] result->atoms[index].element;
    }
    delete[] result->atoms;
  }
  std::memset(&result->unit_cell, 0, sizeof(result->unit_cell));
  result->atoms = nullptr;
  result->atom_count = 0;
  result->estimated_count = 0;
  result->parse_error = KERNEL_CRYSTAL_PARSE_ERROR_NONE;
  result->supercell_error = KERNEL_CRYSTAL_SUPERCELL_ERROR_NONE;
}

void reset_cif_miller_plane_result(kernel_cif_miller_plane_result* result) {
  if (result == nullptr) {
    return;
  }
  std::memset(result, 0, sizeof(kernel_cif_miller_plane_result));
  result->plane.error = KERNEL_CRYSTAL_MILLER_ERROR_NONE;
  result->parse_error = KERNEL_CRYSTAL_PARSE_ERROR_NONE;
}

double deg_to_rad(const double value) {
  return value * kPi / 180.0;
}

bool fill_unit_cell_box(
    const kernel_crystal_cell_params& cell,
    kernel_unit_cell_box* out_unit_cell,
    kernel_crystal_supercell_error* out_error) {
  const double alpha = deg_to_rad(cell.alpha_deg);
  const double beta = deg_to_rad(cell.beta_deg);
  const double gamma = deg_to_rad(cell.gamma_deg);

  const double cos_alpha = std::cos(alpha);
  const double cos_beta = std::cos(beta);
  const double cos_gamma = std::cos(gamma);
  const double sin_gamma = std::sin(gamma);

  if (std::abs(sin_gamma) < kCrystalEpsilon) {
    *out_error = KERNEL_CRYSTAL_SUPERCELL_ERROR_GAMMA_TOO_SMALL;
    return false;
  }

  const double ax = cell.a;
  const double bx = cell.b * cos_gamma;
  const double by = cell.b * sin_gamma;
  const double cx = cell.c * cos_beta;
  const double cy = cell.c * (cos_alpha - cos_beta * cos_gamma) / sin_gamma;
  const double cz2 = cell.c * cell.c - cx * cx - cy * cy;
  if (cz2 < -kCrystalEpsilon) {
    *out_error = KERNEL_CRYSTAL_SUPERCELL_ERROR_INVALID_BASIS;
    return false;
  }

  std::memset(out_unit_cell, 0, sizeof(*out_unit_cell));
  out_unit_cell->a = cell.a;
  out_unit_cell->b = cell.b;
  out_unit_cell->c = cell.c;
  out_unit_cell->alpha_deg = cell.alpha_deg;
  out_unit_cell->beta_deg = cell.beta_deg;
  out_unit_cell->gamma_deg = cell.gamma_deg;
  out_unit_cell->vectors[0][0] = ax;
  out_unit_cell->vectors[0][1] = 0.0;
  out_unit_cell->vectors[0][2] = 0.0;
  out_unit_cell->vectors[1][0] = bx;
  out_unit_cell->vectors[1][1] = by;
  out_unit_cell->vectors[1][2] = 0.0;
  out_unit_cell->vectors[2][0] = cx;
  out_unit_cell->vectors[2][1] = cy;
  out_unit_cell->vectors[2][2] = std::sqrt(std::max(0.0, cz2));
  *out_error = KERNEL_CRYSTAL_SUPERCELL_ERROR_NONE;
  return true;
}

bool fill_supercell_result(
    const std::vector<kernel::crystal::AtomNodeView>& atoms,
    kernel_supercell_result* out_result) {
  if (atoms.empty()) {
    out_result->count = 0;
    out_result->atoms = nullptr;
    return true;
  }

  out_result->atoms = new (std::nothrow) kernel_atom_node[atoms.size()]{};
  if (out_result->atoms == nullptr) {
    return false;
  }
  out_result->count = atoms.size();

  for (std::size_t index = 0; index < atoms.size(); ++index) {
    const auto& source = atoms[index];
    auto& target = out_result->atoms[index];
    target.element = kernel::core::duplicate_c_string(source.element);
    if (target.element == nullptr) {
      return false;
    }
    for (std::size_t coord = 0; coord < 3; ++coord) {
      target.cartesian_coords[coord] = source.cartesian_coords[coord];
    }
  }
  return true;
}

bool fill_lattice_atoms(
    const std::vector<kernel::crystal::AtomNodeView>& atoms,
    kernel_lattice_result* out_result) {
  if (atoms.empty()) {
    out_result->atom_count = 0;
    out_result->atoms = nullptr;
    return true;
  }

  out_result->atoms = new (std::nothrow) kernel_atom_node[atoms.size()] {};
  if (out_result->atoms == nullptr) {
    return false;
  }
  out_result->atom_count = atoms.size();

  for (std::size_t index = 0; index < atoms.size(); ++index) {
    const auto& source = atoms[index];
    auto& target = out_result->atoms[index];
    target.element = kernel::core::duplicate_c_string(source.element);
    if (target.element == nullptr) {
      return false;
    }
    for (std::size_t coord = 0; coord < 3; ++coord) {
      target.cartesian_coords[coord] = source.cartesian_coords[coord];
    }
  }
  return true;
}

bool fill_crystal_parse_result(
    const kernel::crystal::CifParseComputation& parsed,
    kernel_crystal_parse_result* out_result) {
  out_result->cell = parsed.cell;
  out_result->error = parsed.error;

  if (!parsed.atoms.empty()) {
    out_result->atoms = new (std::nothrow) kernel_fractional_atom_record[parsed.atoms.size()]{};
    if (out_result->atoms == nullptr) {
      return false;
    }
    out_result->atom_count = parsed.atoms.size();
    for (std::size_t index = 0; index < parsed.atoms.size(); ++index) {
      const auto& source = parsed.atoms[index];
      auto& target = out_result->atoms[index];
      target.element = kernel::core::duplicate_c_string(source.element);
      if (target.element == nullptr) {
        return false;
      }
      for (std::size_t coord = 0; coord < 3; ++coord) {
        target.frac[coord] = source.frac[coord];
      }
    }
  }

  if (!parsed.symops.empty()) {
    out_result->symops =
        new (std::nothrow) kernel_symmetry_operation_input[parsed.symops.size()]{};
    if (out_result->symops == nullptr) {
      return false;
    }
    out_result->symop_count = parsed.symops.size();
    std::copy(parsed.symops.begin(), parsed.symops.end(), out_result->symops);
  }

  return true;
}

std::vector<kernel_fractional_atom_input> atom_inputs_from_parsed(
    const std::vector<kernel::crystal::FractionalAtomRecord>& atoms) {
  std::vector<kernel_fractional_atom_input> inputs;
  inputs.reserve(atoms.size());
  for (const auto& atom : atoms) {
    kernel_fractional_atom_input input{};
    input.element = atom.element.c_str();
    for (std::size_t coord = 0; coord < 3; ++coord) {
      input.frac[coord] = atom.frac[coord];
    }
    inputs.push_back(input);
  }
  return inputs;
}

}  // namespace

extern "C" kernel_status kernel_parse_cif_crystal(
    const char* raw,
    const size_t raw_size,
    kernel_crystal_parse_result* out_result) {
  reset_crystal_parse_result(out_result);
  if (raw == nullptr || out_result == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const auto parsed = kernel::crystal::parse_cif_crystal(std::string_view(raw, raw_size));
  if (parsed.error != KERNEL_CRYSTAL_PARSE_ERROR_NONE) {
    out_result->error = parsed.error;
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  if (!fill_crystal_parse_result(parsed, out_result)) {
    reset_crystal_parse_result(out_result);
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }

  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_calculate_miller_plane(
    const kernel_crystal_cell_params* cell,
    const int32_t h,
    const int32_t k,
    const int32_t l,
    kernel_miller_plane_result* out_result) {
  reset_miller_plane_result(out_result);
  if (cell == nullptr || out_result == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const auto computation = kernel::crystal::calculate_miller_plane(*cell, h, k, l);
  if (computation.error != KERNEL_CRYSTAL_MILLER_ERROR_NONE) {
    out_result->error = computation.error;
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  *out_result = computation.result;
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_build_supercell(
    const kernel_crystal_cell_params* cell,
    const kernel_fractional_atom_input* atoms,
    const size_t atom_count,
    const kernel_symmetry_operation_input* symops,
    const size_t symop_count,
    const uint32_t nx,
    const uint32_t ny,
    const uint32_t nz,
    kernel_supercell_result* out_result) {
  reset_supercell_result(out_result);
  if (cell == nullptr || out_result == nullptr ||
      (atom_count > 0 && atoms == nullptr) || (symop_count > 0 && symops == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const auto computation =
      kernel::crystal::build_supercell(*cell, atoms, atom_count, symops, symop_count, nx, ny, nz);
  if (computation.error != KERNEL_CRYSTAL_SUPERCELL_ERROR_NONE) {
    out_result->error = computation.error;
    out_result->estimated_count = computation.estimated_count;
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  if (!fill_supercell_result(computation.atoms, out_result)) {
    reset_supercell_result(out_result);
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  out_result->error = KERNEL_CRYSTAL_SUPERCELL_ERROR_NONE;
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_build_lattice_from_cif(
    const char* raw,
    const size_t raw_size,
    const uint32_t nx,
    const uint32_t ny,
    const uint32_t nz,
    kernel_lattice_result* out_result) {
  reset_lattice_result(out_result);
  if (raw == nullptr || out_result == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const auto parsed = kernel::crystal::parse_cif_crystal(std::string_view(raw, raw_size));
  if (parsed.error != KERNEL_CRYSTAL_PARSE_ERROR_NONE) {
    out_result->parse_error = parsed.error;
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_crystal_supercell_error unit_cell_error = KERNEL_CRYSTAL_SUPERCELL_ERROR_NONE;
  if (!fill_unit_cell_box(parsed.cell, &out_result->unit_cell, &unit_cell_error)) {
    out_result->supercell_error = unit_cell_error;
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const auto atom_inputs = atom_inputs_from_parsed(parsed.atoms);
  const kernel_fractional_atom_input* atom_data =
      atom_inputs.empty() ? nullptr : atom_inputs.data();
  const kernel_symmetry_operation_input* symop_data =
      parsed.symops.empty() ? nullptr : parsed.symops.data();
  const auto computation = kernel::crystal::build_supercell(
      parsed.cell,
      atom_data,
      atom_inputs.size(),
      symop_data,
      parsed.symops.size(),
      nx,
      ny,
      nz);
  if (computation.error != KERNEL_CRYSTAL_SUPERCELL_ERROR_NONE) {
    out_result->supercell_error = computation.error;
    out_result->estimated_count = computation.estimated_count;
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  if (!fill_lattice_atoms(computation.atoms, out_result)) {
    reset_lattice_result(out_result);
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  out_result->parse_error = KERNEL_CRYSTAL_PARSE_ERROR_NONE;
  out_result->supercell_error = KERNEL_CRYSTAL_SUPERCELL_ERROR_NONE;
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_calculate_miller_plane_from_cif(
    const char* raw,
    const size_t raw_size,
    const int32_t h,
    const int32_t k,
    const int32_t l,
    kernel_cif_miller_plane_result* out_result) {
  reset_cif_miller_plane_result(out_result);
  if (raw == nullptr || out_result == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const auto parsed = kernel::crystal::parse_cif_crystal(std::string_view(raw, raw_size));
  if (parsed.error != KERNEL_CRYSTAL_PARSE_ERROR_NONE) {
    out_result->parse_error = parsed.error;
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const auto computation = kernel::crystal::calculate_miller_plane(parsed.cell, h, k, l);
  if (computation.error != KERNEL_CRYSTAL_MILLER_ERROR_NONE) {
    out_result->plane.error = computation.error;
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  out_result->plane = computation.result;
  out_result->parse_error = KERNEL_CRYSTAL_PARSE_ERROR_NONE;
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" void kernel_free_supercell_result(kernel_supercell_result* result) {
  reset_supercell_result(result);
}

extern "C" void kernel_free_crystal_parse_result(kernel_crystal_parse_result* result) {
  reset_crystal_parse_result(result);
}

extern "C" void kernel_free_lattice_result(kernel_lattice_result* result) {
  reset_lattice_result(result);
}
