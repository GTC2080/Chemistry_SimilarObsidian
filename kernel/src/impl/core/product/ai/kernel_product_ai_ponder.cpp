// Reason: Own AI ponder prompt rules separately from RAG and embedding rules.

#include "core/kernel_product_ai.h"

#include "core/kernel_product_ai_text.h"

#include <string>
#include <string_view>

namespace {

constexpr float kAiPonderTemperature = 0.7F;
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

}  // namespace

namespace kernel::core::product {

float ai_ponder_temperature() {
  return kAiPonderTemperature;
}

std::string_view ai_ponder_system_prompt() {
  return ai_utf8_literal(kAiPonderSystemPrompt);
}

std::string build_ai_ponder_user_prompt(
    std::string_view topic,
    std::string_view context) {
  std::string prompt;
  prompt.reserve(
      ai_utf8_literal(kAiPonderTopicLabel).size() + topic.size() +
      ai_utf8_literal(kAiPonderContextLabel).size() + context.size() +
      ai_utf8_literal(kAiPonderInstruction).size() + 2);
  prompt.append(ai_utf8_literal(kAiPonderTopicLabel));
  prompt.append(topic);
  prompt.push_back('\n');
  prompt.append(ai_utf8_literal(kAiPonderContextLabel));
  prompt.append(context);
  prompt.push_back('\n');
  prompt.append(ai_utf8_literal(kAiPonderInstruction));
  return prompt;
}

}  // namespace kernel::core::product
