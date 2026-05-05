// Reason: Own AI product limits and note-path shaping separately from compute rules.

#include "core/kernel_product_ai.h"

#include <cstddef>
#include <string_view>

namespace {

constexpr std::size_t kRagContextPerNoteChars = 1500;
constexpr std::size_t kEmbeddingTextCharLimit = 2000;
constexpr std::size_t kAiChatTimeoutSecs = 120;
constexpr std::size_t kAiPonderTimeoutSecs = 60;
constexpr std::size_t kAiEmbeddingRequestTimeoutSecs = 30;
constexpr std::size_t kAiEmbeddingCacheLimit = 64;
constexpr std::size_t kAiEmbeddingConcurrencyLimit = 4;
constexpr std::size_t kAiRagTopNoteLimit = 5;

}  // namespace

namespace kernel::core::product {

std::size_t embedding_text_char_limit() {
  return kEmbeddingTextCharLimit;
}

std::size_t rag_context_per_note_char_limit() {
  return kRagContextPerNoteChars;
}

std::size_t ai_chat_timeout_secs() {
  return kAiChatTimeoutSecs;
}

std::size_t ai_ponder_timeout_secs() {
  return kAiPonderTimeoutSecs;
}

std::size_t ai_embedding_request_timeout_secs() {
  return kAiEmbeddingRequestTimeoutSecs;
}

std::size_t ai_embedding_cache_limit() {
  return kAiEmbeddingCacheLimit;
}

std::size_t ai_embedding_concurrency_limit() {
  return kAiEmbeddingConcurrencyLimit;
}

std::size_t ai_rag_top_note_limit() {
  return kAiRagTopNoteLimit;
}

std::string_view derive_note_display_name_from_path(std::string_view note_path) {
  const std::size_t slash = note_path.find_last_of("/\\");
  const std::size_t name_start = slash == std::string_view::npos ? 0 : slash + 1;
  if (name_start >= note_path.size()) {
    return note_path;
  }

  const std::string_view file_name = note_path.substr(name_start);
  const std::size_t dot = file_name.find_last_of('.');
  if (dot == std::string_view::npos || dot == 0) {
    return file_name;
  }
  return file_name.substr(0, dot);
}

}  // namespace kernel::core::product
