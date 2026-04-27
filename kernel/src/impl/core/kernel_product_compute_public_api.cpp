// Reason: Expose product compute rules through the kernel C ABI so hosts only
// marshal DTOs and keep localized presentation text outside the kernel.

#include "kernel/c_api.h"

#include "core/kernel_shared.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr int kExpPer100Chars = 2;
constexpr int kExpPerMolEdit = 5;
constexpr int kExpPerCodeBlock = 8;
constexpr std::size_t kMinContextChars = 24;
constexpr std::size_t kMaxContextChars = 2200;
constexpr std::size_t kRagContextPerNoteChars = 1500;
constexpr std::size_t kEmbeddingTextCharLimit = 2000;
constexpr double kStudySecsPerExp = 60.0;
constexpr double kStudyBaseExp = 100.0;
constexpr double kStudyGrowthRate = 1.5;
constexpr std::int64_t kStudyAttrExpPerLevel = 50;

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

std::string_view extension_from_note_id(std::string_view note_id) {
  const std::size_t dot = note_id.find_last_of('.');
  if (dot == std::string_view::npos || dot + 1 >= note_id.size()) {
    return {};
  }
  return note_id.substr(dot + 1);
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

bool is_ascii_space(const char ch) {
  const auto byte = static_cast<unsigned char>(ch);
  return std::isspace(byte) != 0;
}

std::string_view trim_start_ascii(std::string_view value) {
  std::size_t start = 0;
  while (start < value.size() && is_ascii_space(value[start])) {
    ++start;
  }
  return value.substr(start);
}

std::string_view trim_ascii(std::string_view value) {
  std::size_t start = 0;
  while (start < value.size() && is_ascii_space(value[start])) {
    ++start;
  }

  std::size_t end = value.size();
  while (end > start && is_ascii_space(value[end - 1])) {
    --end;
  }
  return value.substr(start, end - start);
}

std::string_view trim_to_max(std::string_view value) {
  if (value.size() <= kMaxContextChars) {
    return value;
  }
  return value.substr(value.size() - kMaxContextChars);
}

std::vector<std::string_view> split_lines(std::string_view value) {
  std::vector<std::string_view> lines;
  std::size_t start = 0;
  while (start <= value.size()) {
    const std::size_t next = value.find('\n', start);
    if (next == std::string_view::npos) {
      lines.push_back(value.substr(start));
      break;
    }
    lines.push_back(value.substr(start, next - start));
    start = next + 1;
    if (start == value.size()) {
      break;
    }
  }
  return lines;
}

std::vector<std::string_view> split_blocks(std::string_view value) {
  std::vector<std::string_view> blocks;
  std::size_t start = 0;
  while (start <= value.size()) {
    const std::size_t next = value.find("\n\n", start);
    const std::string_view raw =
        next == std::string_view::npos ? value.substr(start) : value.substr(start, next - start);
    const std::string_view trimmed = trim_ascii(raw);
    if (!trimmed.empty()) {
      blocks.push_back(trimmed);
    }
    if (next == std::string_view::npos) {
      break;
    }
    start = next + 2;
  }
  return blocks;
}

std::string join_views(const std::vector<std::string_view>& values, std::string_view separator) {
  std::string joined;
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index > 0) {
      joined.append(separator);
    }
    joined.append(values[index]);
  }
  return joined;
}

std::string build_semantic_context_impl(std::string_view content) {
  const std::string_view trimmed = trim_ascii(content);
  if (trimmed.size() <= kMaxContextChars) {
    return std::string(trimmed);
  }

  const std::vector<std::string_view> lines = split_lines(trimmed);
  std::vector<std::string_view> headings;
  for (const auto line : lines) {
    const std::string_view candidate = trim_start_ascii(line);
    if (
        candidate.starts_with("# ") || candidate.starts_with("## ") ||
        candidate.starts_with("### ") || candidate.starts_with("#### ")) {
      headings.push_back(line);
    }
  }
  if (headings.size() > 4) {
    headings.erase(headings.begin(), headings.end() - 4);
  }

  std::vector<std::string_view> recent_blocks = split_blocks(trimmed);
  if (recent_blocks.size() > 3) {
    recent_blocks.erase(recent_blocks.begin(), recent_blocks.end() - 3);
  }

  std::vector<std::string> sections;
  if (!headings.empty()) {
    sections.push_back("Headings:\n" + join_views(headings, "\n"));
  }
  if (!recent_blocks.empty()) {
    sections.push_back("Recent focus:\n" + join_views(recent_blocks, "\n\n"));
  }

  std::string joined;
  for (std::size_t index = 0; index < sections.size(); ++index) {
    if (index > 0) {
      joined.append("\n\n");
    }
    joined.append(sections[index]);
  }

  if (joined.size() >= kMinContextChars) {
    return std::string(trim_to_max(joined));
  }
  return std::string(trim_to_max(trimmed));
}

bool fill_owned_buffer(std::string_view value, kernel_owned_buffer* out_buffer) {
  out_buffer->data = nullptr;
  out_buffer->size = 0;
  if (value.empty()) {
    return true;
  }

  auto* owned = new (std::nothrow) char[value.size()];
  if (owned == nullptr) {
    return false;
  }
  std::memcpy(owned, value.data(), value.size());
  out_buffer->data = owned;
  out_buffer->size = value.size();
  return true;
}

kernel_status write_size_limit(std::size_t value, std::size_t* out_value) {
  if (out_value == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  *out_value = value;
  return kernel::core::make_status(KERNEL_OK);
}

std::int64_t calc_study_next_level_exp(const std::int64_t level) {
  const double value =
      kStudyBaseExp * std::pow(kStudyGrowthRate, static_cast<double>(level - 1));
  if (!std::isfinite(value) || value > static_cast<double>(std::numeric_limits<std::int64_t>::max())) {
    return std::numeric_limits<std::int64_t>::max();
  }
  return static_cast<std::int64_t>(std::floor(value));
}

std::int64_t study_attr_level(const std::int64_t exp) {
  return std::min<std::int64_t>(99, 1 + exp / kStudyAttrExpPerLevel);
}

std::int64_t study_secs_to_exp(const std::int64_t secs) {
  return static_cast<std::int64_t>(
      std::floor(static_cast<double>(secs) / kStudySecsPerExp));
}

void add_value_for_attr(
    kernel_truth_attribute_values& values,
    std::string_view attr,
    const std::int64_t value) {
  if (attr == "science") {
    values.science += value;
  } else if (attr == "engineering") {
    values.engineering += value;
  } else if (attr == "finance") {
    values.finance += value;
  } else {
    values.creation += value;
  }
}

kernel_truth_attribute_values attribute_levels_from_exp(
    const kernel_truth_attribute_values& exp) {
  return kernel_truth_attribute_values{
      study_attr_level(exp.science),
      study_attr_level(exp.engineering),
      study_attr_level(exp.creation),
      study_attr_level(exp.finance)};
}

std::int64_t total_exp(const kernel_truth_attribute_values& exp) {
  return exp.science + exp.engineering + exp.creation + exp.finance;
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

extern "C" kernel_status kernel_build_semantic_context(
    const char* content,
    const std::size_t content_size,
    kernel_owned_buffer* out_buffer) {
  if (out_buffer == nullptr || (content_size > 0 && content == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  const std::string_view raw(content == nullptr ? "" : content, content_size);
  const std::string context = build_semantic_context_impl(raw);
  if (!fill_owned_buffer(context, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_get_semantic_context_min_bytes(std::size_t* out_bytes) {
  return write_size_limit(kMinContextChars, out_bytes);
}

extern "C" kernel_status kernel_get_rag_context_per_note_char_limit(std::size_t* out_chars) {
  return write_size_limit(kRagContextPerNoteChars, out_chars);
}

extern "C" kernel_status kernel_get_embedding_text_char_limit(std::size_t* out_chars) {
  return write_size_limit(kEmbeddingTextCharLimit, out_chars);
}

extern "C" kernel_status kernel_compute_truth_state_from_activity(
    const kernel_study_note_activity* activities,
    const std::size_t activity_count,
    kernel_truth_state_snapshot* out_state) {
  if (out_state == nullptr || (activity_count > 0 && activities == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  *out_state = kernel_truth_state_snapshot{};
  kernel_truth_attribute_values secs{};
  for (std::size_t index = 0; index < activity_count; ++index) {
    const kernel_study_note_activity& activity = activities[index];
    if (activity.note_id == nullptr) {
      return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
    }
    add_value_for_attr(
        secs,
        route_by_extension(extension_from_note_id(activity.note_id)),
        activity.active_secs);
  }

  kernel_truth_attribute_values exp{
      study_secs_to_exp(secs.science),
      study_secs_to_exp(secs.engineering),
      study_secs_to_exp(secs.creation),
      study_secs_to_exp(secs.finance)};

  std::int64_t level = 1;
  std::int64_t remaining = total_exp(exp);
  std::int64_t next_level_exp = calc_study_next_level_exp(level);
  while (remaining >= next_level_exp && next_level_exp > 0) {
    remaining -= next_level_exp;
    ++level;
    next_level_exp = calc_study_next_level_exp(level);
  }

  out_state->level = level;
  out_state->total_exp = remaining;
  out_state->next_level_exp = next_level_exp;
  out_state->attributes = attribute_levels_from_exp(exp);
  out_state->attribute_exp = exp;
  return kernel::core::make_status(KERNEL_OK);
}
