// Reason: This file implements the minimal Track 5 Batch 3 chemistry selector
// model so grammar, canonicalization, and basis derivation stay out of storage
// and ABI code.

#include "chemistry/chemistry_spectrum_selector.h"

#include "vault/revision.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdlib>
#include <string_view>

namespace kernel::chemistry {
namespace {

bool is_lower_ascii_token(std::string_view value) {
  if (value.empty()) {
    return false;
  }
  if (!(value.front() >= 'a' && value.front() <= 'z')) {
    return false;
  }
  for (const unsigned char ch : value) {
    if (!(std::isdigit(ch) != 0 || (ch >= 'a' && ch <= 'z') || ch == '.' ||
          ch == '/' || ch == '-')) {
      return false;
    }
  }
  return true;
}

bool parse_double(std::string_view text, double& out_value) {
  std::string owned(text);
  char* parse_end = nullptr;
  out_value = std::strtod(owned.c_str(), &parse_end);
  return parse_end == owned.c_str() + owned.size();
}

std::string strip_trailing_zeroes(std::string value) {
  const std::size_t dot_offset = value.find('.');
  if (dot_offset == std::string::npos) {
    return value;
  }

  while (!value.empty() && value.back() == '0') {
    value.pop_back();
  }
  if (!value.empty() && value.back() == '.') {
    value.pop_back();
  }
  return value;
}

bool parse_prefixed_field(
    std::string_view segment,
    std::string_view prefix,
    std::string& out_value) {
  if (!segment.starts_with(prefix)) {
    return false;
  }
  out_value.assign(segment.substr(prefix.size()));
  return !out_value.empty();
}

}  // namespace

std::string build_normalized_spectrum_basis(const SpectrumMetadata& metadata) {
  return metadata.source_format + "\n" + metadata.x_axis_unit + "\n" +
         metadata.y_axis_unit + "\n" + std::to_string(metadata.point_count);
}

std::string build_chemistry_selector_basis_revision(
    std::string_view attachment_content_revision,
    std::string_view normalized_spectrum_basis,
    std::string_view selector_mode) {
  if (attachment_content_revision.empty() || normalized_spectrum_basis.empty()) {
    return {};
  }

  return kernel::vault::compute_content_revision(
      std::string(attachment_content_revision) + "\n" + std::string(selector_mode) + "\n" +
      std::string(normalized_spectrum_basis));
}

bool normalize_selector_decimal(std::string_view raw_value, std::string& out_value) {
  out_value.clear();
  if (raw_value.empty() || raw_value.find_first_of("eE+") != std::string_view::npos) {
    return false;
  }

  double parsed_value = 0.0;
  if (!parse_double(raw_value, parsed_value)) {
    return false;
  }

  if (parsed_value == 0.0) {
    out_value = "0";
    return true;
  }

  std::string owned(raw_value);
  bool negative = false;
  std::size_t index = 0;
  if (owned[index] == '-') {
    negative = true;
    ++index;
  }

  if (index >= owned.size()) {
    return false;
  }

  std::string integer_part;
  std::string fractional_part;
  const std::size_t dot_offset = owned.find('.', index);
  if (dot_offset == std::string::npos) {
    integer_part = owned.substr(index);
  } else {
    integer_part = owned.substr(index, dot_offset - index);
    fractional_part = owned.substr(dot_offset + 1);
    if (fractional_part.empty()) {
      return false;
    }
  }

  if (integer_part.empty()) {
    return false;
  }
  if (integer_part.size() > 1 && integer_part.front() == '0') {
    return false;
  }
  if (!std::all_of(integer_part.begin(), integer_part.end(), [](unsigned char ch) {
        return std::isdigit(ch) != 0;
      })) {
    return false;
  }
  if (!std::all_of(fractional_part.begin(), fractional_part.end(), [](unsigned char ch) {
        return std::isdigit(ch) != 0;
      })) {
    return false;
  }

  out_value = negative ? "-" + integer_part : integer_part;
  if (!fractional_part.empty()) {
    out_value += ".";
    out_value += fractional_part;
    out_value = strip_trailing_zeroes(std::move(out_value));
  }
  if (out_value == "-0") {
    out_value = "0";
  }
  return true;
}

std::string serialize_chem_spectrum_selector(const ParsedChemSpectrumSelector& selector) {
  if (selector.kind == ChemSpectrumSelectorKind::WholeSpectrum) {
    return "chemsel:v1|kind=whole|basis=" + selector.chemistry_selector_basis_revision;
  }

  return "chemsel:v1|kind=x_range|basis=" + selector.chemistry_selector_basis_revision +
         "|start=" + selector.start + "|end=" + selector.end + "|unit=" + selector.unit;
}

bool parse_chem_spectrum_selector(
    std::string_view serialized_selector,
    ParsedChemSpectrumSelector& out_selector) {
  out_selector = ParsedChemSpectrumSelector{};
  constexpr std::string_view kWholePrefix = "chemsel:v1|kind=whole|basis=";
  constexpr std::string_view kRangePrefix = "chemsel:v1|kind=x_range|basis=";

  if (serialized_selector.starts_with(kWholePrefix)) {
    out_selector.kind = ChemSpectrumSelectorKind::WholeSpectrum;
    out_selector.chemistry_selector_basis_revision.assign(
        serialized_selector.substr(kWholePrefix.size()));
    return !out_selector.chemistry_selector_basis_revision.empty() &&
           serialize_chem_spectrum_selector(out_selector) == serialized_selector;
  }

  if (!serialized_selector.starts_with(kRangePrefix)) {
    return false;
  }

  const std::size_t start_pos = serialized_selector.find("|start=");
  const std::size_t end_pos = serialized_selector.find("|end=");
  const std::size_t unit_pos = serialized_selector.find("|unit=");
  if (start_pos == std::string_view::npos || end_pos == std::string_view::npos ||
      unit_pos == std::string_view::npos || !(start_pos < end_pos && end_pos < unit_pos)) {
    return false;
  }

  out_selector.kind = ChemSpectrumSelectorKind::XRange;
  out_selector.chemistry_selector_basis_revision.assign(
      serialized_selector.substr(kRangePrefix.size(), start_pos - kRangePrefix.size()));
  if (out_selector.chemistry_selector_basis_revision.empty()) {
    return false;
  }

  std::string raw_start;
  std::string raw_end;
  std::string raw_unit;
  if (!parse_prefixed_field(
          serialized_selector.substr(start_pos, end_pos - start_pos),
          "|start=",
          raw_start) ||
      !parse_prefixed_field(
          serialized_selector.substr(end_pos, unit_pos - end_pos),
          "|end=",
          raw_end) ||
      !parse_prefixed_field(
          serialized_selector.substr(unit_pos),
          "|unit=",
          raw_unit)) {
    return false;
  }

  if (!normalize_selector_decimal(raw_start, out_selector.start) ||
      !normalize_selector_decimal(raw_end, out_selector.end) ||
      !is_lower_ascii_token(raw_unit)) {
    return false;
  }
  out_selector.unit = raw_unit;

  double start_value = 0.0;
  double end_value = 0.0;
  if (!parse_double(out_selector.start, start_value) ||
      !parse_double(out_selector.end, end_value) ||
      start_value > end_value) {
    return false;
  }

  return serialize_chem_spectrum_selector(out_selector) == serialized_selector;
}

std::string build_chem_spectrum_selector_preview(const ParsedChemSpectrumSelector& selector) {
  if (selector.kind == ChemSpectrumSelectorKind::WholeSpectrum) {
    return "whole spectrum";
  }
  return selector.unit + " " + selector.start + ".." + selector.end;
}

kernel_chem_spectrum_selector_kind to_public_selector_kind(const ChemSpectrumSelectorKind kind) {
  switch (kind) {
    case ChemSpectrumSelectorKind::WholeSpectrum:
      return KERNEL_CHEM_SPECTRUM_SELECTOR_WHOLE_SPECTRUM;
    case ChemSpectrumSelectorKind::XRange:
    default:
      return KERNEL_CHEM_SPECTRUM_SELECTOR_X_RANGE;
  }
}

}  // namespace kernel::chemistry
