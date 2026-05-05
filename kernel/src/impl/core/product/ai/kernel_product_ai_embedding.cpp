// Reason: Own AI embedding text normalization and cache key rules separately from RAG prompts.

#include "core/kernel_product_ai.h"

#include "core/kernel_product_ai_text.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>

namespace {

constexpr std::uint64_t kFnv1a64Offset = 14695981039346656037ull;
constexpr std::uint64_t kFnv1a64Prime = 1099511628211ull;

void fnv1a_append(std::uint64_t& hash, std::string_view value) {
  for (const unsigned char ch : value) {
    hash ^= static_cast<std::uint64_t>(ch);
    hash *= kFnv1a64Prime;
  }
}

void fnv1a_append_separator(std::uint64_t& hash) {
  hash ^= 0xFFU;
  hash *= kFnv1a64Prime;
}

std::string hex_u64(std::uint64_t value) {
  char buffer[17]{};
  std::snprintf(
      buffer,
      sizeof(buffer),
      "%016llx",
      static_cast<unsigned long long>(value));
  return std::string(buffer);
}

}  // namespace

namespace kernel::core::product {

std::string normalize_ai_embedding_text(std::string_view text) {
  const std::size_t truncated_size =
      ai_utf8_prefix_bytes_by_chars(text, embedding_text_char_limit());
  return std::string(text.substr(0, truncated_size));
}

bool is_ai_embedding_text_indexable(std::string_view text) {
  return ai_has_non_whitespace_utf8(text);
}

std::string compute_ai_embedding_cache_key(
    std::string_view base_url,
    std::string_view model,
    std::string_view text) {
  std::uint64_t hash = kFnv1a64Offset;
  fnv1a_append(hash, base_url);
  fnv1a_append_separator(hash);
  fnv1a_append(hash, model);
  fnv1a_append_separator(hash);
  fnv1a_append(hash, text);
  return hex_u64(hash);
}

}  // namespace kernel::core::product
