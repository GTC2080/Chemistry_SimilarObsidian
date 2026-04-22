// Reason: This file implements the minimal Track 5 Batch 1 chemistry spectra
// metadata extractor so the first chemistry capability stays small and
// registry-driven.

#include "chemistry/chemistry_spectrum_metadata.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace kernel::chemistry {
namespace {

bool is_ascii_space(const char ch) {
  return std::isspace(static_cast<unsigned char>(ch)) != 0;
}

std::string to_lower_ascii(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
}

std::string trim_ascii(std::string_view value) {
  std::size_t start = 0;
  while (start < value.size() && is_ascii_space(value[start])) {
    ++start;
  }

  std::size_t end = value.size();
  while (end > start && is_ascii_space(value[end - 1])) {
    --end;
  }
  return std::string(value.substr(start, end - start));
}

std::string collapse_ascii_whitespace(std::string_view value) {
  std::string output;
  output.reserve(value.size());
  bool previous_was_space = false;
  for (const char ch : value) {
    if (ch == '\r' || ch == '\n' || ch == '\t' || ch == ' ') {
      if (!output.empty() && !previous_was_space) {
        output.push_back(' ');
      }
      previous_was_space = true;
      continue;
    }

    previous_was_space = false;
    output.push_back(ch);
  }
  return trim_ascii(output);
}

std::string normalize_header_key(std::string_view raw_key) {
  std::string key;
  key.reserve(raw_key.size());
  for (const char ch : raw_key) {
    if (std::isalnum(static_cast<unsigned char>(ch)) != 0) {
      key.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
  }
  return key;
}

std::vector<std::string_view> split_lines(std::string_view bytes) {
  std::vector<std::string_view> lines;
  if (bytes.empty()) {
    return lines;
  }

  std::size_t start = 0;
  while (start < bytes.size()) {
    const std::size_t end = bytes.find('\n', start);
    if (end == std::string_view::npos) {
      lines.push_back(bytes.substr(start));
      break;
    }
    std::size_t line_end = end;
    if (line_end > start && bytes[line_end - 1] == '\r') {
      --line_end;
    }
    lines.push_back(bytes.substr(start, line_end - start));
    start = end + 1;
  }
  return lines;
}

bool parse_unsigned_u64(std::string_view value, std::uint64_t& out_value) {
  const std::string trimmed = trim_ascii(value);
  if (trimmed.empty()) {
    return false;
  }

  const auto* begin = trimmed.data();
  const auto* end = trimmed.data() + trimmed.size();
  const auto result = std::from_chars(begin, end, out_value);
  return result.ec == std::errc{} && result.ptr == end;
}

bool parse_decimal_cell(std::string_view value) {
  const std::string trimmed = trim_ascii(value);
  if (trimmed.empty()) {
    return false;
  }

  char* parse_end = nullptr;
  std::strtod(trimmed.c_str(), &parse_end);
  return parse_end == trimmed.c_str() + trimmed.size();
}

bool has_exactly_one_comma(std::string_view value, std::size_t& comma_offset) {
  comma_offset = value.find(',');
  if (comma_offset == std::string_view::npos) {
    return false;
  }
  return value.find(',', comma_offset + 1) == std::string_view::npos;
}

std::string normalize_unit(std::string_view raw_value) {
  std::string value = to_lower_ascii(trim_ascii(raw_value));
  if (value.empty()) {
    return {};
  }

  value.erase(
      std::remove_if(value.begin(), value.end(), [](const char ch) {
        return ch == ' ' || ch == '_';
      }),
      value.end());

  if (value == "cm^-1") {
    return "cm-1";
  }
  if (value == "a.u" || value == "au") {
    return "a.u.";
  }
  if (value == "relativeintensity" || value == "relative-intensity") {
    return "relative-intensity";
  }

  static const std::unordered_map<std::string, std::string> kCanonicalUnits{
      {"ppm", "ppm"},
      {"hz", "hz"},
      {"khz", "khz"},
      {"mhz", "mhz"},
      {"ghz", "ghz"},
      {"cm-1", "cm-1"},
      {"nm", "nm"},
      {"m/z", "m/z"},
      {"a.u.", "a.u."},
      {"absorbance", "absorbance"},
      {"transmittance", "transmittance"},
      {"intensity", "intensity"},
      {"relative-intensity", "relative-intensity"},
  };

  const auto it = kCanonicalUnits.find(value);
  return it == kCanonicalUnits.end() ? std::string{} : it->second;
}

std::string normalize_sample_label(std::string_view raw_value) {
  std::string normalized = collapse_ascii_whitespace(raw_value);
  if (normalized.empty()) {
    return {};
  }
  if (normalized.size() > kChemSpectrumSampleLabelMaxBytes) {
    return {};
  }
  return normalized;
}

std::string normalize_family_token(std::string_view raw_value) {
  const std::string value = to_lower_ascii(trim_ascii(raw_value));
  if (value == "nmr_like") {
    return "nmr_like";
  }
  if (value == "ir_like") {
    return "ir_like";
  }
  if (value == "uv_like") {
    return "uv_like";
  }
  if (value == "ms_like") {
    return "ms_like";
  }
  if (value == "unknown" || value.empty()) {
    return "unknown";
  }
  return {};
}

std::string infer_family_from_text(std::string_view raw_value) {
  const std::string value = to_lower_ascii(collapse_ascii_whitespace(raw_value));
  if (value.find("nmr") != std::string::npos ||
      value.find("nuclear magnetic resonance") != std::string::npos) {
    return "nmr_like";
  }
  if (value.find("infrared") != std::string::npos) {
    return "ir_like";
  }
  if (value.find("ultraviolet") != std::string::npos ||
      value.find("uv") != std::string::npos) {
    return "uv_like";
  }
  if (value.find("mass") != std::string::npos) {
    return "ms_like";
  }
  return "unknown";
}

std::string infer_family_from_units(std::string_view x_axis_unit) {
  const std::string normalized_unit = normalize_unit(x_axis_unit);
  if (normalized_unit == "ppm" || normalized_unit == "hz" ||
      normalized_unit == "khz" || normalized_unit == "mhz" ||
      normalized_unit == "ghz") {
    return "nmr_like";
  }
  if (normalized_unit == "cm-1") {
    return "ir_like";
  }
  if (normalized_unit == "nm") {
    return "uv_like";
  }
  if (normalized_unit == "m/z") {
    return "ms_like";
  }
  return "unknown";
}

SpectrumParseResult extract_jcamp_metadata(
    std::string_view bytes,
    std::string_view attachment_content_revision) {
  SpectrumParseResult result{};
  result.status = SpectrumParseStatus::Unresolved;

  bool has_jcamp_signature = false;
  std::string family = "unknown";
  std::string x_axis_unit;
  std::string y_axis_unit;
  std::string sample_label;
  std::uint64_t point_count = 0;

  for (const std::string_view line : split_lines(bytes)) {
    if (!line.starts_with("##")) {
      continue;
    }

    const std::size_t equals_offset = line.find('=');
    if (equals_offset == std::string_view::npos || equals_offset <= 2) {
      continue;
    }

    const std::string key = normalize_header_key(line.substr(2, equals_offset - 2));
    const std::string_view value = line.substr(equals_offset + 1);
    if (key == "jcampdx") {
      has_jcamp_signature = true;
      continue;
    }
    if (key == "datatype") {
      family = infer_family_from_text(value);
      continue;
    }
    if (key == "xunits") {
      x_axis_unit = normalize_unit(value);
      continue;
    }
    if (key == "yunits") {
      y_axis_unit = normalize_unit(value);
      continue;
    }
    if (key == "npoints") {
      if (!parse_unsigned_u64(value, point_count)) {
        return result;
      }
      continue;
    }
    if (key == "title") {
      sample_label = normalize_sample_label(value);
      continue;
    }
  }

  if (!has_jcamp_signature || x_axis_unit.empty() || y_axis_unit.empty() ||
      point_count == 0) {
    return result;
  }

  if (family == "unknown") {
    family = infer_family_from_units(x_axis_unit);
  }

  result.status = SpectrumParseStatus::Ready;
  result.metadata.attachment_content_revision = std::string(attachment_content_revision);
  result.metadata.source_format = "jcamp_dx";
  result.metadata.family = family;
  result.metadata.x_axis_unit = x_axis_unit;
  result.metadata.y_axis_unit = y_axis_unit;
  result.metadata.point_count = point_count;
  result.metadata.sample_label = sample_label;
  result.metadata.chemistry_metadata_revision =
      build_chemistry_metadata_revision(attachment_content_revision);
  return result;
}

SpectrumParseResult extract_csv_metadata(
    std::string_view bytes,
    std::string_view attachment_content_revision) {
  SpectrumParseResult result{};
  result.status = SpectrumParseStatus::Unresolved;

  std::string family = "unknown";
  std::string x_axis_unit;
  std::string y_axis_unit;
  std::string sample_label;
  bool saw_header = false;
  std::uint64_t point_count = 0;

  for (const std::string_view raw_line : split_lines(bytes)) {
    const std::string trimmed_line = trim_ascii(raw_line);
    if (!saw_header) {
      if (trimmed_line.empty()) {
        continue;
      }

      if (trimmed_line.starts_with('#')) {
        const std::size_t equals_offset = trimmed_line.find('=');
        if (equals_offset == std::string::npos || equals_offset <= 1) {
          return result;
        }

        const std::string key =
            to_lower_ascii(trim_ascii(std::string_view(trimmed_line).substr(1, equals_offset - 1)));
        const std::string_view value =
            std::string_view(trimmed_line).substr(equals_offset + 1);
        if (key == "x_unit") {
          x_axis_unit = normalize_unit(value);
          continue;
        }
        if (key == "y_unit") {
          y_axis_unit = normalize_unit(value);
          continue;
        }
        if (key == "family") {
          family = normalize_family_token(value);
          if (family.empty()) {
            return result;
          }
          continue;
        }
        if (key == "sample_label") {
          sample_label = normalize_sample_label(value);
          continue;
        }
        return result;
      }

      if (trimmed_line != "x,y") {
        return result;
      }

      saw_header = true;
      continue;
    }

    if (trimmed_line.empty() || trimmed_line.starts_with('#')) {
      return result;
    }

    std::size_t comma_offset = 0;
    if (!has_exactly_one_comma(trimmed_line, comma_offset)) {
      return result;
    }

    const std::string_view x_cell = std::string_view(trimmed_line).substr(0, comma_offset);
    const std::string_view y_cell = std::string_view(trimmed_line).substr(comma_offset + 1);
    if (!parse_decimal_cell(x_cell) || !parse_decimal_cell(y_cell)) {
      return result;
    }
    ++point_count;
  }

  if (!saw_header || x_axis_unit.empty() || y_axis_unit.empty() || point_count == 0) {
    return result;
  }

  result.status = SpectrumParseStatus::Ready;
  result.metadata.attachment_content_revision = std::string(attachment_content_revision);
  result.metadata.source_format = "spectrum_csv_v1";
  result.metadata.family = family;
  result.metadata.x_axis_unit = x_axis_unit;
  result.metadata.y_axis_unit = y_axis_unit;
  result.metadata.point_count = point_count;
  result.metadata.sample_label = sample_label;
  result.metadata.chemistry_metadata_revision =
      build_chemistry_metadata_revision(attachment_content_revision);
  return result;
}

}  // namespace

std::string build_chemistry_metadata_revision(std::string_view attachment_content_revision) {
  if (attachment_content_revision.empty()) {
    return {};
  }

  return std::string("chemmeta:v1:") + std::string(attachment_content_revision) + ":" +
         std::string(kChemistryExtractModeRevision);
}

SpectrumParseResult extract_spectrum_metadata(
    std::string_view rel_path,
    std::string_view bytes,
    std::string_view attachment_content_revision) {
  const std::string extension =
      to_lower_ascii(std::filesystem::path(std::string(rel_path)).extension().generic_string());
  if (extension == ".jdx" || extension == ".dx") {
    return extract_jcamp_metadata(bytes, attachment_content_revision);
  }
  if (extension == ".csv") {
    return extract_csv_metadata(bytes, attachment_content_revision);
  }
  return SpectrumParseResult{};
}

}  // namespace kernel::chemistry
