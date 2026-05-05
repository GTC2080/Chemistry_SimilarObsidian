// Reason: Own AI RAG context and system prompt rules separately from embedding rules.

#include "core/kernel_product_ai.h"

#include "core/kernel_product_ai_text.h"

#include <string>
#include <string_view>

namespace {

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

void append_rag_note(
    std::string& context,
    std::size_t& emitted_count,
    std::string_view name,
    std::string_view content) {
  if (!kernel::core::product::ai_has_non_whitespace_utf8(content)) {
    return;
  }
  const std::size_t truncated_size = kernel::core::product::ai_utf8_prefix_bytes_by_chars(
      content,
      kernel::core::product::rag_context_per_note_char_limit());

  context.append(kernel::core::product::ai_utf8_literal(kAiRagNotePrefix));
  ++emitted_count;
  context.append(std::to_string(emitted_count));
  context.append(kernel::core::product::ai_utf8_literal(kAiRagNoteNameOpen));
  context.append(name);
  context.append(kernel::core::product::ai_utf8_literal(kAiRagNoteNameClose));
  context.append(content.substr(0, truncated_size));
  context.append("\n\n");
}

}  // namespace

namespace kernel::core::product {

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
    append_rag_note(context, emitted_count, name, content);
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
    append_rag_note(context, emitted_count, name, content);
  }
  return context;
}

std::string build_ai_rag_system_content(std::string_view context) {
  std::string content;
  content.reserve(
      ai_utf8_literal(kAiRagSystemPrompt).size() +
      ai_utf8_literal(kAiRagContextHeader).size() + context.size() + 4);
  content.append(ai_utf8_literal(kAiRagSystemPrompt));
  content.append("\n\n");
  content.append(ai_utf8_literal(kAiRagContextHeader));
  content.append("\n\n");
  content.append(context);
  return content;
}

}  // namespace kernel::core::product
