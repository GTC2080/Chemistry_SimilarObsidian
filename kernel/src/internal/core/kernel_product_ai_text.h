// Reason: Share AI UTF-8 text helpers across embedding, RAG, and prompt rules.

#pragma once

#include <cstddef>
#include <string_view>

namespace kernel::core::product {

template <std::size_t N>
std::string_view ai_utf8_literal(const char8_t (&value)[N]) {
  return std::string_view(reinterpret_cast<const char*>(value), N - 1);
}

std::size_t ai_utf8_prefix_bytes_by_chars(std::string_view value, std::size_t char_limit);
bool ai_has_non_whitespace_utf8(std::string_view value);

}  // namespace kernel::core::product
