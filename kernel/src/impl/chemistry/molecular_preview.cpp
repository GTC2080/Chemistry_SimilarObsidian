// Reason: This file owns molecular preview construction formerly implemented
// in the Tauri Rust backend.

#include "chemistry/molecular_preview.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace kernel::chemistry {
namespace {

std::string lower_ascii(std::string_view value) {
  std::string out(value);
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char ch) {
    if (ch >= 'A' && ch <= 'Z') {
      return static_cast<char>(ch - 'A' + 'a');
    }
    return static_cast<char>(ch);
  });
  return out;
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

bool is_blank(std::string_view value) {
  return value.find_first_not_of(" \t\r\n") == std::string_view::npos;
}

bool starts_with(std::string_view value, std::string_view prefix) {
  return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

std::string join_lines(const std::vector<std::string>& lines) {
  std::string out;
  for (std::size_t index = 0; index < lines.size(); ++index) {
    if (index > 0) {
      out.push_back('\n');
    }
    out += lines[index];
  }
  return out;
}

MolecularPreviewComputation build_pdb_preview(std::string_view raw, const std::size_t max_atoms) {
  MolecularPreviewComputation result{};
  std::vector<std::string> lines;

  for (const std::string_view line : split_lines(raw)) {
    const bool is_atom_line = starts_with(line, "ATOM") || starts_with(line, "HETATM");
    if (is_atom_line) {
      ++result.atom_count;
      if (result.preview_atom_count < max_atoms) {
        ++result.preview_atom_count;
        lines.emplace_back(line);
      }
      continue;
    }
    lines.emplace_back(line);
  }

  result.preview_atom_count = std::min(result.preview_atom_count, result.atom_count);
  result.truncated = result.atom_count > max_atoms;
  result.preview_data = join_lines(lines);
  return result;
}

MolecularPreviewComputation build_xyz_preview(std::string_view raw, const std::size_t max_atoms) {
  MolecularPreviewComputation result{};
  const auto lines = split_lines(raw);
  const std::string_view comment = lines.size() > 1 ? lines[1] : std::string_view{};

  std::vector<std::string_view> atom_lines;
  for (std::size_t index = 2; index < lines.size(); ++index) {
    if (!is_blank(lines[index])) {
      atom_lines.push_back(lines[index]);
    }
  }

  result.atom_count = atom_lines.size();
  result.preview_atom_count = std::min(result.atom_count, max_atoms);
  result.truncated = result.atom_count > max_atoms;

  std::vector<std::string> preview_lines;
  preview_lines.reserve(result.preview_atom_count + 2);
  preview_lines.push_back(std::to_string(result.preview_atom_count));
  preview_lines.emplace_back(comment);
  for (std::size_t index = 0; index < result.preview_atom_count; ++index) {
    preview_lines.emplace_back(atom_lines[index]);
  }

  result.preview_data = join_lines(preview_lines);
  return result;
}

MolecularPreviewComputation build_cif_preview(std::string_view raw) {
  MolecularPreviewComputation result{};
  result.preview_data = std::string(raw);
  return result;
}

}  // namespace

MolecularPreviewComputation build_molecular_preview(
    std::string_view raw,
    std::string_view extension,
    const std::size_t max_atoms) {
  const std::string ext = lower_ascii(extension);
  if (ext == "pdb") {
    return build_pdb_preview(raw, max_atoms);
  }
  if (ext == "xyz") {
    return build_xyz_preview(raw, max_atoms);
  }
  if (ext == "cif") {
    return build_cif_preview(raw);
  }

  MolecularPreviewComputation result{};
  result.error = KERNEL_MOLECULAR_PREVIEW_ERROR_UNSUPPORTED_EXTENSION;
  return result;
}

}  // namespace kernel::chemistry
