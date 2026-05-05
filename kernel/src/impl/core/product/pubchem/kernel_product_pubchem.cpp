// Reason: Own PubChem query and payload shaping away from the public ABI wrapper.

#include "core/kernel_product_pubchem.h"

#include "core/kernel_shared.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>

namespace {

std::uint32_t utf8_codepoint_at(
    std::string_view value,
    const std::size_t offset,
    std::size_t& width) {
  const auto byte0 = static_cast<unsigned char>(value[offset]);
  if (byte0 < 0x80U) {
    width = 1;
    return byte0;
  }
  if ((byte0 & 0xE0U) == 0xC0U && offset + 1 < value.size()) {
    const auto byte1 = static_cast<unsigned char>(value[offset + 1]);
    if ((byte1 & 0xC0U) == 0x80U) {
      width = 2;
      return ((byte0 & 0x1FU) << 6U) | (byte1 & 0x3FU);
    }
  }
  if ((byte0 & 0xF0U) == 0xE0U && offset + 2 < value.size()) {
    const auto byte1 = static_cast<unsigned char>(value[offset + 1]);
    const auto byte2 = static_cast<unsigned char>(value[offset + 2]);
    if ((byte1 & 0xC0U) == 0x80U && (byte2 & 0xC0U) == 0x80U) {
      width = 3;
      return ((byte0 & 0x0FU) << 12U) | ((byte1 & 0x3FU) << 6U) | (byte2 & 0x3FU);
    }
  }
  if ((byte0 & 0xF8U) == 0xF0U && offset + 3 < value.size()) {
    const auto byte1 = static_cast<unsigned char>(value[offset + 1]);
    const auto byte2 = static_cast<unsigned char>(value[offset + 2]);
    const auto byte3 = static_cast<unsigned char>(value[offset + 3]);
    if (
        (byte1 & 0xC0U) == 0x80U && (byte2 & 0xC0U) == 0x80U &&
        (byte3 & 0xC0U) == 0x80U) {
      width = 4;
      return (
          ((byte0 & 0x07U) << 18U) | ((byte1 & 0x3FU) << 12U) |
          ((byte2 & 0x3FU) << 6U) | (byte3 & 0x3FU));
    }
  }
  width = 1;
  return byte0;
}

bool is_unicode_whitespace(const std::uint32_t codepoint) {
  return (
      codepoint == 0x09U || codepoint == 0x0AU || codepoint == 0x0BU ||
      codepoint == 0x0CU || codepoint == 0x0DU || codepoint == 0x20U ||
      codepoint == 0x85U || codepoint == 0xA0U || codepoint == 0x1680U ||
      (codepoint >= 0x2000U && codepoint <= 0x200AU) || codepoint == 0x2028U ||
      codepoint == 0x2029U || codepoint == 0x202FU || codepoint == 0x205FU ||
      codepoint == 0x3000U);
}

std::string_view trim_utf8_whitespace(std::string_view value) {
  std::size_t start = 0;
  while (start < value.size()) {
    std::size_t width = 1;
    const std::uint32_t codepoint = utf8_codepoint_at(value, start, width);
    if (!is_unicode_whitespace(codepoint)) {
      break;
    }
    start += width;
  }

  std::size_t end = value.size();
  while (end > start) {
    std::size_t previous = end - 1;
    while (previous > start && (static_cast<unsigned char>(value[previous]) & 0xC0U) == 0x80U) {
      --previous;
    }
    std::size_t width = 1;
    const std::uint32_t codepoint = utf8_codepoint_at(value, previous, width);
    if (previous + width != end || !is_unicode_whitespace(codepoint)) {
      break;
    }
    end = previous;
  }

  return value.substr(start, end - start);
}

std::string json_string(std::string_view value) {
  return "\"" + kernel::core::json_escape(value) + "\"";
}

std::string json_double(const double value) {
  char buffer[64]{};
  const int count = std::snprintf(buffer, sizeof(buffer), "%.15g", value);
  if (count <= 0) {
    return "0";
  }
  return std::string(buffer, static_cast<std::size_t>(count));
}

std::string pubchem_status_json(std::string_view status) {
  return "{\"status\":" + json_string(status) + "}";
}

}  // namespace

namespace kernel::core::product {

std::string normalize_pubchem_query(std::string_view query) {
  return std::string(trim_utf8_whitespace(query));
}

std::string build_pubchem_compound_info_payload_json(
    std::string_view query,
    std::string_view formula,
    const double molecular_weight,
    const bool has_density,
    const double density,
    const std::size_t property_count) {
  const std::string_view normalized_query = trim_utf8_whitespace(query);
  if (normalized_query.empty()) {
    return pubchem_status_json("emptyQuery");
  }
  if (property_count == 0) {
    return pubchem_status_json("notFound");
  }
  if (property_count > 1) {
    return pubchem_status_json("ambiguous");
  }

  const std::string_view normalized_formula = trim_utf8_whitespace(formula);
  if (
      normalized_formula.empty() || !std::isfinite(molecular_weight) ||
      molecular_weight <= 0.0) {
    return pubchem_status_json("notFound");
  }

  std::string output = "{\"status\":\"ok\",\"name\":";
  output += json_string(normalized_query);
  output += ",\"formula\":";
  output += json_string(normalized_formula);
  output += ",\"molecularWeight\":";
  output += json_double(molecular_weight);
  output += ",\"density\":";
  if (has_density && std::isfinite(density) && density > 0.0) {
    output += json_double(density);
  } else {
    output += "null";
  }
  output += "}";
  return output;
}

}  // namespace kernel::core::product
