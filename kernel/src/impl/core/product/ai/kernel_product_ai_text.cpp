// Reason: Own AI UTF-8 scanning separately from embedding and RAG rules.

#include "core/kernel_product_ai_text.h"

#include <cstdint>
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

}  // namespace

namespace kernel::core::product {

std::size_t ai_utf8_prefix_bytes_by_chars(
    std::string_view value,
    const std::size_t char_limit) {
  std::size_t offset = 0;
  std::size_t chars = 0;
  while (offset < value.size() && chars < char_limit) {
    std::size_t width = 1;
    (void)utf8_codepoint_at(value, offset, width);
    offset += width;
    ++chars;
  }
  return offset;
}

bool ai_has_non_whitespace_utf8(std::string_view value) {
  std::size_t offset = 0;
  while (offset < value.size()) {
    std::size_t width = 1;
    const std::uint32_t codepoint = utf8_codepoint_at(value, offset, width);
    if (!is_unicode_whitespace(codepoint)) {
      return true;
    }
    offset += width;
  }
  return false;
}

}  // namespace kernel::core::product
