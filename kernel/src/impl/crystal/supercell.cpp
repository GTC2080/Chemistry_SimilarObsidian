// Reason: This file owns crystal symmetry expansion and supercell Cartesian
// coordinate generation formerly implemented in the Tauri Rust backend.

#include "crystal/supercell.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

namespace kernel::crystal {
namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr double kGridScale = 50.0;

struct LatticeVectors {
  double vectors[3][3]{};
  kernel_crystal_supercell_error error = KERNEL_CRYSTAL_SUPERCELL_ERROR_NONE;
};

struct FractionalAtomView {
  std::string element;
  double frac[3]{};
};

struct GridKey {
  std::string element;
  std::int64_t x = 0;
  std::int64_t y = 0;
  std::int64_t z = 0;

  bool operator==(const GridKey& other) const {
    return element == other.element && x == other.x && y == other.y && z == other.z;
  }
};

struct GridKeyHash {
  std::size_t operator()(const GridKey& key) const {
    std::size_t seed = std::hash<std::string>{}(key.element);
    seed ^= std::hash<std::int64_t>{}(key.x) + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    seed ^= std::hash<std::int64_t>{}(key.y) + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    seed ^= std::hash<std::int64_t>{}(key.z) + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    return seed;
  }
};

double radians(const double degrees) {
  return degrees * kPi / 180.0;
}

double rem_euclid_one(const double value) {
  double result = std::fmod(value, 1.0);
  if (result < 0.0) {
    result += 1.0;
  }
  return result;
}

std::int64_t quantize_frac(const double value) {
  const auto grid = static_cast<std::int64_t>(kGridScale);
  const auto quantized = static_cast<std::int64_t>(std::llround(rem_euclid_one(value) * kGridScale));
  return quantized % grid;
}

GridKey grid_key(const char* element, const double frac[3]) {
  return GridKey{
      element == nullptr ? std::string{} : std::string(element),
      quantize_frac(frac[0]),
      quantize_frac(frac[1]),
      quantize_frac(frac[2])};
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
    result.error = KERNEL_CRYSTAL_SUPERCELL_ERROR_GAMMA_TOO_SMALL;
    return result;
  }

  const double ax = cell.a;
  const double bx = cell.b * cos_gamma;
  const double by = cell.b * sin_gamma;
  const double cx = cell.c * cos_beta;
  const double cy = cell.c * (cos_alpha - cos_beta * cos_gamma) / sin_gamma;
  const double cz2 = cell.c * cell.c - cx * cx - cy * cy;
  if (cz2 < -1.0e-8) {
    result.error = KERNEL_CRYSTAL_SUPERCELL_ERROR_INVALID_BASIS;
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

void frac_to_cart(const double frac[3], const double vectors[3][3], double out[3]) {
  out[0] = frac[0] * vectors[0][0] + frac[1] * vectors[1][0] + frac[2] * vectors[2][0];
  out[1] = frac[0] * vectors[0][1] + frac[1] * vectors[1][1] + frac[2] * vectors[2][1];
  out[2] = frac[0] * vectors[0][2] + frac[1] * vectors[1][2] + frac[2] * vectors[2][2];
}

}  // namespace

SupercellComputation build_supercell(
    const kernel_crystal_cell_params& cell,
    const kernel_fractional_atom_input* atoms,
    const std::size_t atom_count,
    const kernel_symmetry_operation_input* symops,
    const std::size_t symop_count,
    const std::uint32_t nx,
    const std::uint32_t ny,
    const std::uint32_t nz) {
  SupercellComputation computation{};

  const LatticeVectors lattice = build_lattice_vectors(cell);
  if (lattice.error != KERNEL_CRYSTAL_SUPERCELL_ERROR_NONE) {
    computation.error = lattice.error;
    return computation;
  }

  const std::size_t capacity = atom_count * symop_count;
  std::unordered_set<GridKey, GridKeyHash> seen;
  seen.reserve(capacity);
  std::vector<FractionalAtomView> unit_atoms;
  unit_atoms.reserve(capacity);

  for (std::size_t atom_index = 0; atom_index < atom_count; ++atom_index) {
    const auto& atom = atoms[atom_index];
    for (std::size_t op_index = 0; op_index < symop_count; ++op_index) {
      const auto& op = symops[op_index];
      double new_frac[3]{};
      for (int row = 0; row < 3; ++row) {
        new_frac[row] =
            op.rot[row][0] * atom.frac[0] + op.rot[row][1] * atom.frac[1] +
            op.rot[row][2] * atom.frac[2] + op.trans[row];
        new_frac[row] = rem_euclid_one(new_frac[row]);
      }

      GridKey key = grid_key(atom.element, new_frac);
      if (seen.insert(key).second) {
        FractionalAtomView unit_atom{};
        unit_atom.element = atom.element == nullptr ? std::string{} : std::string(atom.element);
        unit_atom.frac[0] = new_frac[0];
        unit_atom.frac[1] = new_frac[1];
        unit_atom.frac[2] = new_frac[2];
        unit_atoms.push_back(std::move(unit_atom));
      }
    }
  }

  const std::size_t total_estimate =
      unit_atoms.size() * static_cast<std::size_t>(nx) * static_cast<std::size_t>(ny) *
      static_cast<std::size_t>(nz);
  computation.estimated_count = static_cast<std::uint64_t>(total_estimate);
  if (total_estimate > kMaxSupercellAtoms) {
    computation.error = KERNEL_CRYSTAL_SUPERCELL_ERROR_TOO_MANY_ATOMS;
    return computation;
  }

  computation.atoms.reserve(total_estimate);
  for (std::uint32_t ix = 0; ix < nx; ++ix) {
    for (std::uint32_t iy = 0; iy < ny; ++iy) {
      for (std::uint32_t iz = 0; iz < nz; ++iz) {
        for (const auto& atom : unit_atoms) {
          AtomNodeView out_atom{};
          out_atom.element = atom.element;
          const double shifted_frac[3] = {
              atom.frac[0] + static_cast<double>(ix),
              atom.frac[1] + static_cast<double>(iy),
              atom.frac[2] + static_cast<double>(iz)};
          frac_to_cart(shifted_frac, lattice.vectors, out_atom.cartesian_coords);
          computation.atoms.push_back(std::move(out_atom));
        }
      }
    }
  }

  computation.error = KERNEL_CRYSTAL_SUPERCELL_ERROR_NONE;
  return computation;
}

}  // namespace kernel::crystal
