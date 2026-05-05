// Reason: Own AI/RAG product compute rules away from the public ABI wrapper.

#include "core/kernel_product_ai.h"

#include <cstdint>
#include <cstdio>
#include <string>
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
constexpr std::uint64_t kFnv1a64Offset = 14695981039346656037ull;
constexpr std::uint64_t kFnv1a64Prime = 1099511628211ull;
constexpr float kAiPonderTemperature = 0.7F;

constexpr char8_t kAiRagSystemPrompt[] =
    u8"\u4F60\u662F\u4E00\u4E2A\u79C1\u4EBA\u77E5\u8BC6\u5E93\u7684"
    u8"\u6781\u5BA2\u52A9\u624B\u3002\u8BF7\u4E25\u683C\u57FA\u4E8E"
    u8"\u4EE5\u4E0B\u63D0\u4F9B\u7684\u4E0A\u4E0B\u6587\u56DE\u7B54"
    u8"\u7528\u6237\u95EE\u9898\u3002\u5982\u679C\u4E0A\u4E0B\u6587"
    u8"\u4E2D\u6CA1\u6709\u7B54\u6848\uFF0C\u8BF7\u8BDA\u5B9E\u5730"
    u8"\u8BF4\u660E\u3002\u8BF7\u5728\u5F15\u7528\u76F8\u5173\u5185"
    u8"\u5BB9\u65F6\uFF0C\u5728\u53E5\u672B\u4F7F\u7528 [[\u7B14"
    u8"\u8BB0\u540D\u79F0]] \u7684\u683C\u5F0F\u6807\u6CE8\u51FA"
    u8"\u5904\u3002";
constexpr char8_t kAiRagContextHeader[] =
    u8"\u4EE5\u4E0B\u662F\u76F8\u5173\u7B14\u8BB0\u4E0A\u4E0B"
    u8"\u6587\uFF1A";
constexpr char8_t kAiRagNotePrefix[] = u8"--- \u7B14\u8BB0 ";
constexpr char8_t kAiRagNoteNameOpen[] = u8" \u300A";
constexpr char8_t kAiRagNoteNameClose[] = u8"\u300B ---\n";
constexpr char8_t kAiPonderSystemPrompt[] =
    u8"\u4F60\u662F\u4E00\u4E2A\u903B\u8F91\u53D1\u6563\u5F15"
    u8"\u64CE\u3002\u4F60\u7684\u4EFB\u52A1\u662F\u56F4\u7ED5"
    u8"\u6838\u5FC3\u6982\u5FF5\u751F\u6210\u53EF\u62D3\u5C55"
    u8"\u77E5\u8BC6\u56FE\u8C31\u7684\u5B50\u8282\u70B9\u3002"
    u8"\u4F60\u5FC5\u987B\u8F93\u51FA\u4E25\u683C JSON \u6570"
    u8"\u7EC4\uFF0C\u4E14\u6570\u7EC4\u5143\u7D20\u7ED3\u6784"
    u8"\u56FA\u5B9A\u4E3A {\"title\":\"...\",\"relation\":\"...\"}"
    u8"\u3002\u7981\u6B62\u8F93\u51FA Markdown\u3001\u4EE3\u7801"
    u8"\u5757\u3001\u89E3\u91CA\u6027\u6587\u672C\u3001\u524D"
    u8"\u540E\u7F00\u3002";
constexpr char8_t kAiPonderTopicLabel[] = u8"\u6838\u5FC3\u6982\u5FF5: ";
constexpr char8_t kAiPonderContextLabel[] = u8"\u4E0A\u4E0B\u6587: ";
constexpr char8_t kAiPonderInstruction[] =
    u8"\u8BF7\u751F\u6210 3 \u5230 5 \u4E2A\u5177\u5907\u903B\u8F91"
    u8"\u9012\u8FDB\u6216\u8865\u5145\u5173\u7CFB\u7684\u5B50\u8282"
    u8"\u70B9\u3002";

template <std::size_t N>
std::string_view utf8_literal(const char8_t (&value)[N]) {
  return std::string_view(reinterpret_cast<const char*>(value), N - 1);
}

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

std::size_t utf8_prefix_bytes_by_chars(std::string_view value, const std::size_t char_limit) {
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

bool is_unicode_whitespace(const std::uint32_t codepoint) {
  return (
      codepoint == 0x09U || codepoint == 0x0AU || codepoint == 0x0BU ||
      codepoint == 0x0CU || codepoint == 0x0DU || codepoint == 0x20U ||
      codepoint == 0x85U || codepoint == 0xA0U || codepoint == 0x1680U ||
      (codepoint >= 0x2000U && codepoint <= 0x200AU) || codepoint == 0x2028U ||
      codepoint == 0x2029U || codepoint == 0x202FU || codepoint == 0x205FU ||
      codepoint == 0x3000U);
}

bool has_non_whitespace_utf8(std::string_view value) {
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

float ai_ponder_temperature() {
  return kAiPonderTemperature;
}

std::string_view ai_ponder_system_prompt() {
  return utf8_literal(kAiPonderSystemPrompt);
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

std::string normalize_ai_embedding_text(std::string_view text) {
  const std::size_t truncated_size =
      utf8_prefix_bytes_by_chars(text, kEmbeddingTextCharLimit);
  return std::string(text.substr(0, truncated_size));
}

bool is_ai_embedding_text_indexable(std::string_view text) {
  return has_non_whitespace_utf8(text);
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

std::string build_ai_rag_context(
    const char* const* note_names,
    const std::size_t* note_name_sizes,
    const char* const* note_contents,
    const std::size_t* note_content_sizes,
    const std::size_t note_count) {
  std::string context;
  std::size_t emitted_count = 0;
  for (std::size_t index = 0; index < note_count; ++index) {
    const std::string_view name(
        note_names[index] == nullptr ? "" : note_names[index],
        note_name_sizes[index]);
    const std::string_view content(
        note_contents[index] == nullptr ? "" : note_contents[index],
        note_content_sizes[index]);
    if (!has_non_whitespace_utf8(content)) {
      continue;
    }
    const std::size_t truncated_size =
        utf8_prefix_bytes_by_chars(content, kRagContextPerNoteChars);

    context.append(utf8_literal(kAiRagNotePrefix));
    ++emitted_count;
    context.append(std::to_string(emitted_count));
    context.append(utf8_literal(kAiRagNoteNameOpen));
    context.append(name);
    context.append(utf8_literal(kAiRagNoteNameClose));
    context.append(content.substr(0, truncated_size));
    context.append("\n\n");
  }
  return context;
}

std::string build_ai_rag_context_from_note_paths(
    const char* const* note_paths,
    const std::size_t* note_path_sizes,
    const char* const* note_contents,
    const std::size_t* note_content_sizes,
    const std::size_t note_count) {
  std::string context;
  std::size_t emitted_count = 0;
  for (std::size_t index = 0; index < note_count; ++index) {
    const std::string_view path(
        note_paths[index] == nullptr ? "" : note_paths[index],
        note_path_sizes[index]);
    const std::string_view name = derive_note_display_name_from_path(path);
    const std::string_view content(
        note_contents[index] == nullptr ? "" : note_contents[index],
        note_content_sizes[index]);
    if (!has_non_whitespace_utf8(content)) {
      continue;
    }
    const std::size_t truncated_size =
        utf8_prefix_bytes_by_chars(content, kRagContextPerNoteChars);

    context.append(utf8_literal(kAiRagNotePrefix));
    ++emitted_count;
    context.append(std::to_string(emitted_count));
    context.append(utf8_literal(kAiRagNoteNameOpen));
    context.append(name);
    context.append(utf8_literal(kAiRagNoteNameClose));
    context.append(content.substr(0, truncated_size));
    context.append("\n\n");
  }
  return context;
}

std::string build_ai_rag_system_content(std::string_view context) {
  std::string content;
  content.reserve(
      utf8_literal(kAiRagSystemPrompt).size() +
      utf8_literal(kAiRagContextHeader).size() + context.size() + 4);
  content.append(utf8_literal(kAiRagSystemPrompt));
  content.append("\n\n");
  content.append(utf8_literal(kAiRagContextHeader));
  content.append("\n\n");
  content.append(context);
  return content;
}

std::string build_ai_ponder_user_prompt(
    std::string_view topic,
    std::string_view context) {
  std::string prompt;
  prompt.reserve(
      utf8_literal(kAiPonderTopicLabel).size() + topic.size() +
      utf8_literal(kAiPonderContextLabel).size() + context.size() +
      utf8_literal(kAiPonderInstruction).size() + 2);
  prompt.append(utf8_literal(kAiPonderTopicLabel));
  prompt.append(topic);
  prompt.push_back('\n');
  prompt.append(utf8_literal(kAiPonderContextLabel));
  prompt.append(context);
  prompt.push_back('\n');
  prompt.append(utf8_literal(kAiPonderInstruction));
  return prompt;
}

}  // namespace kernel::core::product
