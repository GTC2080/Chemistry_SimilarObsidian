// Reason: This file owns the CIF cell/atom/symmetry parser formerly
// implemented in the Tauri Rust backend.

#include "crystal/cif_parser.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace kernel::crystal {
namespace {

std::string_view trim_view(std::string_view value) {
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) {
    value.remove_prefix(1);
  }
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
    value.remove_suffix(1);
  }
  return value;
}

bool starts_with(std::string_view value, std::string_view prefix) {
  return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

bool contains(std::string_view value, std::string_view needle) {
  return value.find(needle) != std::string_view::npos;
}

std::vector<std::string_view> split_lines(std::string_view raw) {
  std::vector<std::string_view> lines;
  std::size_t start = 0;
  while (start <= raw.size()) {
    const std::size_t end = raw.find('\n', start);
    std::string_view line =
        end == std::string_view::npos ? raw.substr(start) : raw.substr(start, end - start);
    if (!line.empty() && line.back() == '\r') {
      line.remove_suffix(1);
    }
    lines.push_back(line);
    if (end == std::string_view::npos) {
      break;
    }
    start = end + 1;
  }
  return lines;
}

std::vector<std::string_view> split_whitespace(std::string_view value) {
  std::vector<std::string_view> parts;
  std::size_t index = 0;
  while (index < value.size()) {
    while (index < value.size() &&
           std::isspace(static_cast<unsigned char>(value[index])) != 0) {
      ++index;
    }
    const std::size_t start = index;
    while (index < value.size() &&
           std::isspace(static_cast<unsigned char>(value[index])) == 0) {
      ++index;
    }
    if (start < index) {
      parts.push_back(value.substr(start, index - start));
    }
  }
  return parts;
}

std::vector<std::string_view> split_whitespace_limited(
    std::string_view value,
    const std::size_t max_parts) {
  std::vector<std::string_view> parts;
  if (max_parts == 0) {
    return parts;
  }

  std::size_t index = 0;
  while (index < value.size()) {
    while (index < value.size() &&
           std::isspace(static_cast<unsigned char>(value[index])) != 0) {
      ++index;
    }
    if (index >= value.size()) {
      break;
    }
    const std::size_t start = index;
    if (parts.size() + 1 == max_parts) {
      parts.push_back(trim_view(value.substr(start)));
      break;
    }
    while (index < value.size() &&
           std::isspace(static_cast<unsigned char>(value[index])) == 0) {
      ++index;
    }
    parts.push_back(value.substr(start, index - start));
  }
  return parts;
}

std::vector<std::string_view> split_commas(std::string_view value) {
  std::vector<std::string_view> parts;
  std::size_t start = 0;
  while (start <= value.size()) {
    const std::size_t end = value.find(',', start);
    parts.push_back(trim_view(
        end == std::string_view::npos ? value.substr(start) : value.substr(start, end - start)));
    if (end == std::string_view::npos) {
      break;
    }
    start = end + 1;
  }
  return parts;
}

std::string_view trim_quotes(std::string_view value) {
  value = trim_view(value);
  if (value.size() >= 2 &&
      ((value.front() == '\'' && value.back() == '\'') ||
       (value.front() == '"' && value.back() == '"'))) {
    value.remove_prefix(1);
    value.remove_suffix(1);
  }
  return value;
}

std::optional<double> parse_cif_number(std::string_view raw) {
  std::string_view trimmed = trim_quotes(raw);
  const std::size_t uncertainty = trimmed.find('(');
  if (uncertainty != std::string_view::npos) {
    trimmed = trimmed.substr(0, uncertainty);
  }
  trimmed = trim_view(trimmed);
  if (trimmed.empty()) {
    return std::nullopt;
  }

  try {
    std::string owned(trimmed);
    std::size_t parsed = 0;
    const double value = std::stod(owned, &parsed);
    if (parsed != owned.size()) {
      return std::nullopt;
    }
    return value;
  } catch (const std::invalid_argument&) {
    return std::nullopt;
  } catch (const std::out_of_range&) {
    return std::nullopt;
  }
}

std::optional<double> parse_tag_value(
    const std::vector<std::string_view>& lines,
    const std::size_t index,
    std::string_view tag) {
  const std::string_view line = trim_view(lines[index]);
  if (!starts_with(line, tag)) {
    return std::nullopt;
  }
  std::string_view rest = trim_view(line.substr(tag.size()));
  if (!rest.empty()) {
    const auto parts = split_whitespace(rest);
    return parse_cif_number(parts.empty() ? rest : parts.front());
  }

  std::size_t next_index = index + 1;
  while (next_index < lines.size()) {
    const std::string_view next = trim_view(lines[next_index]);
    if (next.empty() || starts_with(next, "#")) {
      ++next_index;
      continue;
    }
    if (starts_with(next, "_") || next == "loop_") {
      return std::nullopt;
    }
    const auto parts = split_whitespace(next);
    return parse_cif_number(parts.empty() ? next : parts.front());
  }
  return std::nullopt;
}

std::optional<kernel_crystal_cell_params> parse_cell_params(
    const std::vector<std::string_view>& lines) {
  std::optional<double> a;
  std::optional<double> b;
  std::optional<double> c;
  std::optional<double> alpha;
  std::optional<double> beta;
  std::optional<double> gamma;

  for (std::size_t index = 0; index < lines.size(); ++index) {
    const std::string_view line = trim_view(lines[index]);
    if (starts_with(line, "_cell_length_a")) {
      a = parse_tag_value(lines, index, "_cell_length_a");
    } else if (starts_with(line, "_cell_length_b")) {
      b = parse_tag_value(lines, index, "_cell_length_b");
    } else if (starts_with(line, "_cell_length_c")) {
      c = parse_tag_value(lines, index, "_cell_length_c");
    } else if (starts_with(line, "_cell_angle_alpha")) {
      alpha = parse_tag_value(lines, index, "_cell_angle_alpha");
    } else if (starts_with(line, "_cell_angle_beta")) {
      beta = parse_tag_value(lines, index, "_cell_angle_beta");
    } else if (starts_with(line, "_cell_angle_gamma")) {
      gamma = parse_tag_value(lines, index, "_cell_angle_gamma");
    }
  }

  if (!a || !b || !c || !alpha || !beta || !gamma) {
    return std::nullopt;
  }
  kernel_crystal_cell_params cell{};
  cell.a = *a;
  cell.b = *b;
  cell.c = *c;
  cell.alpha_deg = *alpha;
  cell.beta_deg = *beta;
  cell.gamma_deg = *gamma;
  return cell;
}

std::string normalize_element(std::string_view raw) {
  std::string letters;
  for (const char ch : raw) {
    if (std::isalpha(static_cast<unsigned char>(ch)) != 0) {
      letters.push_back(ch);
    }
  }
  if (letters.empty()) {
    return "X";
  }

  std::string normalized;
  normalized.push_back(
      static_cast<char>(std::toupper(static_cast<unsigned char>(letters.front()))));
  if (letters.size() > 1) {
    normalized.push_back(
        static_cast<char>(std::tolower(static_cast<unsigned char>(letters[1]))));
  }
  return normalized;
}

std::vector<FractionalAtomRecord> parse_atom_sites(const std::vector<std::string_view>& lines) {
  std::vector<FractionalAtomRecord> atoms;
  std::size_t index = 0;

  while (index < lines.size()) {
    const std::string_view line = trim_view(lines[index]);
    if (line == "loop_") {
      std::optional<std::size_t> col_symbol;
      std::optional<std::size_t> col_x;
      std::optional<std::size_t> col_y;
      std::optional<std::size_t> col_z;
      std::size_t col_index = 0;
      const std::size_t header_start = index + 1;
      ++index;

      while (index < lines.size() && starts_with(trim_view(lines[index]), "_")) {
        const std::string_view field = trim_view(lines[index]);
        if ((contains(field, "type_symbol") || field == "_atom_site_label") && !col_symbol) {
          col_symbol = col_index;
        }
        if (contains(field, "_atom_site_fract_x")) {
          col_x = col_index;
        }
        if (contains(field, "_atom_site_fract_y")) {
          col_y = col_index;
        }
        if (contains(field, "_atom_site_fract_z")) {
          col_z = col_index;
        }
        ++col_index;
        ++index;
      }

      bool is_atom_loop = false;
      for (std::size_t header = header_start; header < index; ++header) {
        if (starts_with(trim_view(lines[header]), "_atom_site")) {
          is_atom_loop = true;
          break;
        }
      }

      if (is_atom_loop && col_symbol && col_x && col_y && col_z) {
        while (index < lines.size()) {
          const std::string_view data = trim_view(lines[index]);
          if (data.empty() || starts_with(data, "_") || starts_with(data, "#") ||
              data == "loop_") {
            break;
          }
          const auto parts = split_whitespace(data);
          const std::size_t max_col =
              std::max({*col_symbol, *col_x, *col_y, *col_z});
          if (parts.size() > max_col) {
            const auto x = parse_cif_number(parts[*col_x]);
            const auto y = parse_cif_number(parts[*col_y]);
            const auto z = parse_cif_number(parts[*col_z]);
            if (x && y && z) {
              FractionalAtomRecord atom{};
              atom.element = normalize_element(parts[*col_symbol]);
              atom.frac[0] = *x;
              atom.frac[1] = *y;
              atom.frac[2] = *z;
              atoms.push_back(std::move(atom));
            }
          }
          ++index;
        }
      } else if (is_atom_loop) {
        while (index < lines.size()) {
          const std::string_view data = trim_view(lines[index]);
          if (data.empty() || starts_with(data, "_") || data == "loop_") {
            break;
          }
          ++index;
        }
      }
      continue;
    }
    ++index;
  }

  return atoms;
}

std::optional<std::pair<std::array<double, 3>, double>> parse_symop_component(
    std::string_view input) {
  std::array<double, 3> coeff{};
  double constant = 0.0;
  std::size_t index = 0;

  while (index < input.size()) {
    double sign = 1.0;
    if (input[index] == '-') {
      sign = -1.0;
      ++index;
    } else if (input[index] == '+') {
      ++index;
    }
    if (index >= input.size()) {
      break;
    }

    const char ch = input[index];
    if (ch == 'x' || ch == 'y' || ch == 'z') {
      const std::size_t axis = ch == 'x' ? 0 : (ch == 'y' ? 1 : 2);
      coeff[axis] = sign;
      ++index;
      continue;
    }

    if (std::isdigit(static_cast<unsigned char>(ch)) != 0) {
      const std::size_t start = index;
      while (index < input.size() &&
             (std::isdigit(static_cast<unsigned char>(input[index])) != 0 ||
              input[index] == '.')) {
        ++index;
      }
      const auto number = parse_cif_number(input.substr(start, index - start));
      if (!number) {
        return std::nullopt;
      }

      if (index < input.size() && input[index] == '/') {
        ++index;
        const std::size_t den_start = index;
        while (index < input.size() &&
               std::isdigit(static_cast<unsigned char>(input[index])) != 0) {
          ++index;
        }
        const auto denominator = parse_cif_number(input.substr(den_start, index - den_start));
        if (!denominator || std::abs(*denominator) < 1.0e-12) {
          return std::nullopt;
        }
        constant += sign * (*number) / (*denominator);
        continue;
      }

      if (index < input.size() &&
          (input[index] == 'x' || input[index] == 'y' || input[index] == 'z')) {
        const char axis_char = input[index];
        const std::size_t axis = axis_char == 'x' ? 0 : (axis_char == 'y' ? 1 : 2);
        coeff[axis] = sign * (*number);
        ++index;
      } else {
        constant += sign * (*number);
      }
      continue;
    }

    ++index;
  }

  return std::make_pair(coeff, constant);
}

std::optional<kernel_symmetry_operation_input> parse_symop_xyz(std::string_view xyz) {
  const auto parts = split_commas(xyz);
  if (parts.size() != 3) {
    return std::nullopt;
  }

  kernel_symmetry_operation_input op{};
  for (std::size_t row = 0; row < 3; ++row) {
    std::string component(parts[row]);
    component.erase(
        std::remove_if(
            component.begin(),
            component.end(),
            [](const unsigned char ch) { return std::isspace(ch) != 0; }),
        component.end());
    std::transform(component.begin(), component.end(), component.begin(), [](unsigned char ch) {
      return static_cast<char>(std::tolower(ch));
    });

    const auto parsed = parse_symop_component(component);
    if (!parsed) {
      return std::nullopt;
    }
    for (std::size_t col = 0; col < 3; ++col) {
      op.rot[row][col] = parsed->first[col];
    }
    op.trans[row] = parsed->second;
  }
  return op;
}

std::vector<kernel_symmetry_operation_input> parse_symmetry_ops(
    const std::vector<std::string_view>& lines) {
  std::vector<kernel_symmetry_operation_input> ops;
  std::size_t index = 0;

  while (index < lines.size()) {
    const std::string_view line = trim_view(lines[index]);
    if (line == "loop_") {
      std::optional<std::size_t> col_xyz;
      std::size_t col_index = 0;
      ++index;

      while (index < lines.size() && starts_with(trim_view(lines[index]), "_")) {
        const std::string_view field = trim_view(lines[index]);
        if (contains(field, "_symmetry_equiv_pos_as_xyz") ||
            contains(field, "_space_group_symop_operation_xyz")) {
          col_xyz = col_index;
        }
        ++col_index;
        ++index;
      }

      if (col_xyz) {
        while (index < lines.size()) {
          const std::string_view data = trim_view(lines[index]);
          if (data.empty() || starts_with(data, "_") || starts_with(data, "#") ||
              data == "loop_") {
            break;
          }
          const std::size_t max_parts = std::max(col_index, *col_xyz + 1);
          const auto parts = split_whitespace_limited(data, max_parts);
          if (parts.size() > *col_xyz) {
            const std::string_view xyz = trim_quotes(parts[*col_xyz]);
            if (const auto op = parse_symop_xyz(xyz)) {
              ops.push_back(*op);
            }
          }
          ++index;
        }
      }
      continue;
    }
    ++index;
  }

  if (ops.empty()) {
    kernel_symmetry_operation_input identity{};
    identity.rot[0][0] = 1.0;
    identity.rot[1][1] = 1.0;
    identity.rot[2][2] = 1.0;
    ops.push_back(identity);
  }
  return ops;
}

}  // namespace

CifParseComputation parse_cif_crystal(std::string_view raw) {
  CifParseComputation result{};
  const auto lines = split_lines(raw);

  const auto cell = parse_cell_params(lines);
  if (!cell) {
    result.error = KERNEL_CRYSTAL_PARSE_ERROR_MISSING_CELL;
    return result;
  }

  result.cell = *cell;
  result.atoms = parse_atom_sites(lines);
  if (result.atoms.empty()) {
    result.error = KERNEL_CRYSTAL_PARSE_ERROR_MISSING_ATOMS;
    return result;
  }

  result.symops = parse_symmetry_ops(lines);
  result.error = KERNEL_CRYSTAL_PARSE_ERROR_NONE;
  return result;
}

}  // namespace kernel::crystal
