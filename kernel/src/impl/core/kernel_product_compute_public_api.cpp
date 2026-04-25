// Reason: Expose product compute rules through the kernel C ABI so hosts only
// marshal DTOs and keep localized presentation text outside the kernel.

#include "kernel/c_api.h"

#include "core/kernel_shared.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <new>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr int kExpPer100Chars = 2;
constexpr int kExpPerMolEdit = 5;
constexpr int kExpPerCodeBlock = 8;

struct TruthAwardDraft {
  std::string attr;
  std::int32_t amount = 0;
  kernel_truth_award_reason reason = KERNEL_TRUTH_AWARD_REASON_TEXT_DELTA;
  std::string detail;
};

void reset_truth_diff_result_impl(kernel_truth_diff_result* result) {
  if (result == nullptr) {
    return;
  }
  if (result->awards != nullptr) {
    for (std::size_t index = 0; index < result->count; ++index) {
      delete[] result->awards[index].attr;
      delete[] result->awards[index].detail;
      result->awards[index].attr = nullptr;
      result->awards[index].detail = nullptr;
    }
    delete[] result->awards;
  }
  result->awards = nullptr;
  result->count = 0;
}

bool is_ascii_word(const char ch) {
  const auto byte = static_cast<unsigned char>(ch);
  return std::isalnum(byte) != 0 || ch == '_';
}

std::string to_lower_ascii(std::string_view value) {
  std::string lowered;
  lowered.reserve(value.size());
  for (const char ch : value) {
    lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
  }
  return lowered;
}

std::string route_by_extension(std::string_view ext) {
  const std::string lower = to_lower_ascii(ext);
  if (lower == "jdx" || lower == "csv") {
    return "science";
  }
  if (
      lower == "py" || lower == "js" || lower == "ts" || lower == "tsx" ||
      lower == "jsx" || lower == "rs" || lower == "go" || lower == "c" ||
      lower == "cpp" || lower == "java") {
    return "engineering";
  }
  if (lower == "mol" || lower == "chemdraw") {
    return "creation";
  }
  if (lower == "dashboard" || lower == "base") {
    return "finance";
  }
  return "creation";
}

std::string route_by_code_language(std::string_view lang) {
  const std::string lower = to_lower_ascii(lang);
  if (
      lower == "python" || lower == "py" || lower == "rust" || lower == "go" ||
      lower == "javascript" || lower == "js" || lower == "typescript" ||
      lower == "ts" || lower == "java" || lower == "c" || lower == "cpp") {
    return "engineering";
  }
  if (lower == "smiles" || lower == "chemical" || lower == "latex" || lower == "math") {
    return "science";
  }
  if (lower == "sql" || lower == "r" || lower == "stata") {
    return "finance";
  }
  return {};
}

std::set<std::string> extract_code_languages(std::string_view content) {
  std::set<std::string> languages;
  std::size_t cursor = 0;
  while (cursor < content.size()) {
    const std::size_t fence = content.find("```", cursor);
    if (fence == std::string_view::npos) {
      break;
    }

    std::size_t lang_start = fence + 3;
    std::size_t lang_end = lang_start;
    while (lang_end < content.size() && is_ascii_word(content[lang_end])) {
      ++lang_end;
    }

    if (lang_end > lang_start) {
      languages.insert(to_lower_ascii(content.substr(lang_start, lang_end - lang_start)));
    }
    cursor = lang_end > lang_start ? lang_end : fence + 3;
  }
  return languages;
}

std::size_t line_count(std::string_view content) {
  if (content.empty()) {
    return 0;
  }

  std::size_t count = 0;
  std::size_t start = 0;
  while (start < content.size()) {
    const std::size_t next = content.find('\n', start);
    ++count;
    if (next == std::string_view::npos) {
      break;
    }
    start = next + 1;
    if (start == content.size()) {
      break;
    }
  }
  return count;
}

bool fill_truth_diff_result(
    const std::vector<TruthAwardDraft>& drafts,
    kernel_truth_diff_result* out_result) {
  if (drafts.empty()) {
    return true;
  }

  out_result->awards = new (std::nothrow) kernel_truth_award[drafts.size()]{};
  if (out_result->awards == nullptr) {
    return false;
  }
  out_result->count = drafts.size();

  for (std::size_t index = 0; index < drafts.size(); ++index) {
    const auto& draft = drafts[index];
    auto& target = out_result->awards[index];
    target.attr = kernel::core::duplicate_c_string(draft.attr);
    target.amount = draft.amount;
    target.reason = draft.reason;
    target.detail = draft.detail.empty() ? nullptr : kernel::core::duplicate_c_string(draft.detail);
    if (target.attr == nullptr || (!draft.detail.empty() && target.detail == nullptr)) {
      return false;
    }
  }

  return true;
}

}  // namespace

extern "C" kernel_status kernel_compute_truth_diff(
    const char* prev_content,
    const std::size_t prev_size,
    const char* curr_content,
    const std::size_t curr_size,
    const char* file_extension,
    kernel_truth_diff_result* out_result) {
  if (out_result == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  reset_truth_diff_result_impl(out_result);

  if (
      (prev_size > 0 && prev_content == nullptr) ||
      (curr_size > 0 && curr_content == nullptr) || file_extension == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string_view prev(prev_content == nullptr ? "" : prev_content, prev_size);
  const std::string_view curr(curr_content == nullptr ? "" : curr_content, curr_size);
  if (prev.empty() || curr.empty()) {
    return kernel::core::make_status(KERNEL_OK);
  }

  std::vector<TruthAwardDraft> awards;
  if (curr_size > prev_size) {
    const std::size_t delta = curr_size - prev_size;
    if (delta > 10) {
      const auto char_exp = static_cast<std::int32_t>(
          std::floor((static_cast<double>(delta) / 100.0) * kExpPer100Chars));
      if (char_exp > 0) {
        awards.push_back(TruthAwardDraft{
            route_by_extension(file_extension),
            char_exp,
            KERNEL_TRUTH_AWARD_REASON_TEXT_DELTA,
            {}});
      }
    }
  }

  const std::set<std::string> new_blocks = extract_code_languages(curr);
  const std::set<std::string> old_blocks = extract_code_languages(prev);
  for (const auto& lang : new_blocks) {
    if (old_blocks.contains(lang)) {
      continue;
    }
    const std::string attr = route_by_code_language(lang);
    if (!attr.empty()) {
      awards.push_back(TruthAwardDraft{
          attr,
          kExpPerCodeBlock,
          KERNEL_TRUTH_AWARD_REASON_CODE_LANGUAGE,
          lang});
    }
  }

  const std::string lower_ext = to_lower_ascii(file_extension);
  if (lower_ext == "mol" || lower_ext == "chemdraw") {
    const std::size_t prev_lines = line_count(prev);
    const std::size_t curr_lines = line_count(curr);
    if (curr_lines > prev_lines) {
      awards.push_back(TruthAwardDraft{
          "creation",
          static_cast<std::int32_t>((curr_lines - prev_lines) * kExpPerMolEdit),
          KERNEL_TRUTH_AWARD_REASON_MOLECULAR_EDIT,
          {}});
    }
  }

  if (!fill_truth_diff_result(awards, out_result)) {
    reset_truth_diff_result_impl(out_result);
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }

  return kernel::core::make_status(KERNEL_OK);
}

extern "C" void kernel_free_truth_diff_result(kernel_truth_diff_result* result) {
  reset_truth_diff_result_impl(result);
}
