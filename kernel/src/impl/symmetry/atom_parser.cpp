// Reason: This file owns symmetry atom parsing rules formerly implemented in
// the Tauri Rust backend.

#include "symmetry/atom_parser.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace kernel::symmetry {
namespace {

struct CifCell {
  double a = 0.0;
  double b = 0.0;
  double c = 0.0;
  double alpha_deg = 0.0;
  double beta_deg = 0.0;
  double gamma_deg = 0.0;
};

std::string trim(std::string_view value) {
  std::size_t begin = 0;
  while (begin < value.size() &&
         std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
    ++begin;
  }
  std::size_t end = value.size();
  while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
    --end;
  }
  return std::string(value.substr(begin, end - begin));
}

std::string to_lower_ascii(std::string_view value) {
  std::string lower;
  lower.reserve(value.size());
  for (const unsigned char ch : value) {
    lower.push_back(static_cast<char>(std::tolower(ch)));
  }
  return lower;
}

std::vector<std::string> lines_of(std::string_view raw) {
  std::vector<std::string> lines;
  if (raw.empty()) {
    return lines;
  }
  std::size_t start = 0;
  while (start < raw.size()) {
    const std::size_t end = raw.find('\n', start);
    std::string line = end == std::string_view::npos
                           ? std::string(raw.substr(start))
                           : std::string(raw.substr(start, end - start));
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    lines.push_back(std::move(line));
    if (end == std::string_view::npos) {
      break;
    }
    start = end + 1;
  }
  return lines;
}

std::vector<std::string> split_whitespace(std::string_view line) {
  std::istringstream input{std::string(line)};
  std::vector<std::string> parts;
  std::string part;
  while (input >> part) {
    parts.push_back(part);
  }
  return parts;
}

bool starts_with(std::string_view value, std::string_view prefix) {
  return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

std::optional<double> parse_number(std::string_view value) {
  std::string trimmed = trim(value);
  if (trimmed.empty()) {
    return std::nullopt;
  }
  char* end = nullptr;
  const double parsed = std::strtod(trimmed.c_str(), &end);
  if (end == trimmed.c_str() || !std::isfinite(parsed)) {
    return std::nullopt;
  }
  return parsed;
}

std::optional<double> parse_cif_number(std::string_view value) {
  std::string trimmed = trim(value);
  while (!trimmed.empty() && (trimmed.front() == '\'' || trimmed.front() == '"')) {
    trimmed.erase(trimmed.begin());
  }
  while (!trimmed.empty() && (trimmed.back() == '\'' || trimmed.back() == '"')) {
    trimmed.pop_back();
  }
  const std::size_t paren = trimmed.find('(');
  if (paren != std::string::npos) {
    trimmed.erase(paren);
  }
  return parse_number(trimmed);
}

std::optional<double> parse_cif_tag_value(
    const std::vector<std::string>& lines,
    const std::size_t index,
    std::string_view tag) {
  const std::string line = trim(lines[index]);
  if (!starts_with(line, tag)) {
    return std::nullopt;
  }

  const std::string rest = trim(std::string_view(line).substr(tag.size()));
  if (!rest.empty()) {
    const auto parts = split_whitespace(rest);
    return parse_cif_number(parts.empty() ? rest : parts[0]);
  }

  for (std::size_t next_index = index + 1; next_index < lines.size(); ++next_index) {
    const std::string next = trim(lines[next_index]);
    if (next.empty() || starts_with(next, "#")) {
      continue;
    }
    if (starts_with(next, "_") || next == "loop_" || starts_with(next, ";")) {
      return std::nullopt;
    }
    const auto parts = split_whitespace(next);
    return parse_cif_number(parts.empty() ? next : parts[0]);
  }
  return std::nullopt;
}

std::optional<CifCell> parse_cif_cell(const std::vector<std::string>& lines) {
  std::optional<double> a;
  std::optional<double> b;
  std::optional<double> c;
  std::optional<double> alpha;
  std::optional<double> beta;
  std::optional<double> gamma;

  for (std::size_t index = 0; index < lines.size(); ++index) {
    const std::string line = trim(lines[index]);
    if (starts_with(line, "_cell_length_a")) {
      a = parse_cif_tag_value(lines, index, "_cell_length_a");
    } else if (starts_with(line, "_cell_length_b")) {
      b = parse_cif_tag_value(lines, index, "_cell_length_b");
    } else if (starts_with(line, "_cell_length_c")) {
      c = parse_cif_tag_value(lines, index, "_cell_length_c");
    } else if (starts_with(line, "_cell_angle_alpha")) {
      alpha = parse_cif_tag_value(lines, index, "_cell_angle_alpha");
    } else if (starts_with(line, "_cell_angle_beta")) {
      beta = parse_cif_tag_value(lines, index, "_cell_angle_beta");
    } else if (starts_with(line, "_cell_angle_gamma")) {
      gamma = parse_cif_tag_value(lines, index, "_cell_angle_gamma");
    }
  }

  if (!a || !b || !c || !alpha || !beta || !gamma) {
    return std::nullopt;
  }

  return CifCell{*a, *b, *c, *alpha, *beta, *gamma};
}

std::optional<std::array<double, 3>> frac_to_cart(
    const CifCell& cell,
    const double x,
    const double y,
    const double z) {
  const double alpha = cell.alpha_deg * 3.14159265358979323846 / 180.0;
  const double beta = cell.beta_deg * 3.14159265358979323846 / 180.0;
  const double gamma = cell.gamma_deg * 3.14159265358979323846 / 180.0;
  const double cos_alpha = std::cos(alpha);
  const double cos_beta = std::cos(beta);
  const double cos_gamma = std::cos(gamma);
  const double sin_gamma = std::sin(gamma);

  if (std::abs(sin_gamma) < 1.0e-8) {
    return std::nullopt;
  }

  const double ax = cell.a;
  const double bx = cell.b * cos_gamma;
  const double by = cell.b * sin_gamma;
  const double cx = cell.c * cos_beta;
  const double cy = cell.c * (cos_alpha - cos_beta * cos_gamma) / sin_gamma;
  const double cz2 = cell.c * cell.c - cx * cx - cy * cy;
  if (cz2 < -1.0e-8) {
    return std::nullopt;
  }
  const double cz = std::sqrt(std::max(0.0, cz2));

  return std::array<double, 3>{
      x * ax + y * bx + z * cx,
      y * by + z * cy,
      z * cz,
  };
}

std::string normalize_element(std::string_view raw) {
  std::string letters;
  for (const unsigned char ch : raw) {
    if (std::isalpha(ch) != 0) {
      letters.push_back(static_cast<char>(ch));
    }
  }
  if (letters.empty()) {
    return "X";
  }

  std::string result;
  result.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(letters[0]))));
  if (letters.size() > 1) {
    result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(letters[1]))));
  }
  return result;
}

double atomic_mass(std::string_view element) {
  struct MassEntry {
    std::string_view symbol;
    double mass;
  };
  static constexpr MassEntry masses[] = {
      {"H", 1.008},   {"He", 4.003},  {"Li", 6.941}, {"Be", 9.012},
      {"B", 10.81},   {"C", 12.011},  {"N", 14.007}, {"O", 15.999},
      {"F", 18.998},  {"Ne", 20.180}, {"Na", 22.990}, {"Mg", 24.305},
      {"Al", 26.982}, {"Si", 28.086}, {"P", 30.974}, {"S", 32.065},
      {"Cl", 35.453}, {"Ar", 39.948}, {"K", 39.098}, {"Ca", 40.078},
      {"Ti", 47.867}, {"V", 50.942},  {"Cr", 51.996}, {"Mn", 54.938},
      {"Fe", 55.845}, {"Co", 58.933}, {"Ni", 58.693}, {"Cu", 63.546},
      {"Zn", 65.38},  {"Ga", 69.723}, {"Ge", 72.63},  {"As", 74.922},
      {"Se", 78.971}, {"Br", 79.904}, {"Kr", 83.798}, {"Rb", 85.468},
      {"Sr", 87.62},  {"Zr", 91.224}, {"Mo", 95.95},  {"Ru", 101.07},
      {"Rh", 102.91}, {"Pd", 106.42}, {"Ag", 107.87}, {"Cd", 112.41},
      {"In", 114.82}, {"Sn", 118.71}, {"Sb", 121.76}, {"Te", 127.60},
      {"I", 126.90},  {"Xe", 131.29}, {"Cs", 132.91}, {"Ba", 137.33},
      {"La", 138.91}, {"Pt", 195.08}, {"Au", 196.97}, {"Hg", 200.59},
      {"Pb", 207.2},  {"Bi", 208.98}, {"U", 238.03},
  };
  for (const auto& entry : masses) {
    if (entry.symbol == element) {
      return entry.mass;
    }
  }
  return 12.0;
}

SymmetryAtom atom(std::string element, const double x, const double y, const double z) {
  SymmetryAtom result;
  result.element = std::move(element);
  result.position[0] = x;
  result.position[1] = y;
  result.position[2] = z;
  result.mass = atomic_mass(result.element);
  return result;
}

SymmetryAtomParseResult parse_xyz(std::string_view raw) {
  SymmetryAtomParseResult result;
  const auto lines = lines_of(raw);
  if (lines.empty() || trim(lines[0]).empty()) {
    result.error = KERNEL_SYMMETRY_PARSE_ERROR_XYZ_EMPTY;
    return result;
  }
  if (lines.size() < 2) {
    result.error = KERNEL_SYMMETRY_PARSE_ERROR_XYZ_INCOMPLETE;
    return result;
  }

  for (std::size_t index = 2; index < lines.size(); ++index) {
    const std::string line = trim(lines[index]);
    if (line.empty()) {
      continue;
    }
    const auto parts = split_whitespace(line);
    if (parts.size() < 4) {
      continue;
    }
    const auto x = parse_number(parts[1]);
    const auto y = parse_number(parts[2]);
    const auto z = parse_number(parts[3]);
    if (!x || !y || !z) {
      result.atoms.clear();
      result.error = KERNEL_SYMMETRY_PARSE_ERROR_XYZ_COORDINATE;
      return result;
    }
    result.atoms.push_back(atom(normalize_element(parts[0]), *x, *y, *z));
  }
  return result;
}

SymmetryAtomParseResult parse_pdb(std::string_view raw) {
  SymmetryAtomParseResult result;
  for (const auto& line : lines_of(raw)) {
    if (!starts_with(line, "ATOM") && !starts_with(line, "HETATM")) {
      continue;
    }
    if (line.size() < 54) {
      continue;
    }
    const auto x = parse_number(std::string_view(line).substr(30, 8));
    const auto y = parse_number(std::string_view(line).substr(38, 8));
    const auto z = parse_number(std::string_view(line).substr(46, 8));
    if (!x || !y || !z) {
      result.atoms.clear();
      result.error = KERNEL_SYMMETRY_PARSE_ERROR_PDB_COORDINATE;
      return result;
    }

    std::string element;
    if (line.size() >= 78) {
      element = trim(std::string_view(line).substr(76, 2));
    } else if (line.size() >= 16) {
      element = trim(std::string_view(line).substr(12, 4));
    }
    result.atoms.push_back(atom(normalize_element(element), *x, *y, *z));
  }
  return result;
}

SymmetryAtomParseResult parse_cif(std::string_view raw) {
  SymmetryAtomParseResult result;
  const auto lines = lines_of(raw);
  const auto cell = parse_cif_cell(lines);

  std::size_t index = 0;
  while (index < lines.size()) {
    const std::string line = trim(lines[index]);
    if (line != "loop_") {
      ++index;
      continue;
    }

    std::optional<std::size_t> col_symbol;
    std::optional<std::size_t> col_x;
    std::optional<std::size_t> col_y;
    std::optional<std::size_t> col_z;
    bool uses_fractional = false;
    std::size_t col_index = 0;
    ++index;

    while (index < lines.size() && starts_with(trim(lines[index]), "_")) {
      const std::string field = trim(lines[index]);
      if (field.find("type_symbol") != std::string::npos ||
          field == "_atom_site_label") {
        if (!col_symbol) {
          col_symbol = col_index;
        }
      }
      if (field.find("_atom_site_fract_x") != std::string::npos) {
        col_x = col_index;
        uses_fractional = true;
      } else if (field.find("_atom_site_Cartn_x") != std::string::npos) {
        col_x = col_index;
      }
      if (field.find("_atom_site_fract_y") != std::string::npos) {
        col_y = col_index;
        uses_fractional = true;
      } else if (field.find("_atom_site_Cartn_y") != std::string::npos) {
        col_y = col_index;
      }
      if (field.find("_atom_site_fract_z") != std::string::npos) {
        col_z = col_index;
        uses_fractional = true;
      } else if (field.find("_atom_site_Cartn_z") != std::string::npos) {
        col_z = col_index;
      }
      ++col_index;
      ++index;
    }

    if (col_symbol && col_x && col_y && col_z) {
      if (uses_fractional && !cell) {
        result.atoms.clear();
        result.error = KERNEL_SYMMETRY_PARSE_ERROR_CIF_MISSING_CELL;
        return result;
      }

      while (index < lines.size()) {
        const std::string data_line = trim(lines[index]);
        if (
            data_line.empty() || starts_with(data_line, "_") ||
            starts_with(data_line, "#") || data_line == "loop_") {
          break;
        }
        const auto parts = split_whitespace(data_line);
        const std::size_t max_col = std::max({*col_symbol, *col_x, *col_y, *col_z});
        if (parts.size() > max_col) {
          const auto x = parse_cif_number(parts[*col_x]);
          const auto y = parse_cif_number(parts[*col_y]);
          const auto z = parse_cif_number(parts[*col_z]);
          if (x && y && z) {
            if (uses_fractional) {
              const auto cart = frac_to_cart(*cell, *x, *y, *z);
              if (!cart) {
                result.atoms.clear();
                result.error = KERNEL_SYMMETRY_PARSE_ERROR_CIF_INVALID_CELL;
                return result;
              }
              result.atoms.push_back(atom(
                  normalize_element(parts[*col_symbol]),
                  (*cart)[0],
                  (*cart)[1],
                  (*cart)[2]));
            } else {
              result.atoms.push_back(
                  atom(normalize_element(parts[*col_symbol]), *x, *y, *z));
            }
          }
        }
        ++index;
      }
    }
  }

  return result;
}

}  // namespace

SymmetryAtomParseResult parse_symmetry_atoms_text(
    std::string_view raw,
    std::string_view format) {
  const std::string lower_format = to_lower_ascii(trim(format));
  if (lower_format == "xyz") {
    return parse_xyz(raw);
  }
  if (lower_format == "pdb") {
    return parse_pdb(raw);
  }
  if (lower_format == "cif") {
    return parse_cif(raw);
  }

  SymmetryAtomParseResult result;
  result.error = KERNEL_SYMMETRY_PARSE_ERROR_UNSUPPORTED_FORMAT;
  return result;
}

}  // namespace kernel::symmetry
