// Reason: Expose product compute rules through the kernel C ABI so hosts only
// marshal DTOs and keep localized presentation text outside the kernel.

#include "kernel/c_api.h"

#include "core/kernel_shared.h"
#include "core/kernel_product_ai.h"
#include "core/kernel_product_database.h"
#include "core/kernel_product_paper.h"
#include "core/kernel_product_pubchem.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

constexpr int kExpPer100Chars = 2;
constexpr int kExpPerMolEdit = 5;
constexpr int kExpPerCodeBlock = 8;
constexpr std::size_t kMinContextChars = 24;
constexpr std::size_t kMaxContextChars = 2200;
constexpr double kStudySecsPerExp = 60.0;
constexpr double kStudyBaseExp = 100.0;
constexpr double kStudyGrowthRate = 1.5;
constexpr std::int64_t kStudyAttrExpPerLevel = 50;
constexpr std::int64_t kSecsPerDay = 86400;
constexpr std::size_t kStudyHeatmapWeeks = 26;
constexpr std::size_t kStudyHeatmapDaysPerWeek = 7;
constexpr std::int64_t kStudyWeekLookbackDays = 6;
constexpr std::int64_t kStudyLegacyHeatmapLookbackDays = 179;
constexpr std::size_t kStudyFolderRankLimit = 5;

bool is_ascii_space(char ch);
std::string_view trim_ascii(std::string_view value);


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

void reset_heatmap_grid_impl(kernel_heatmap_grid* grid) {
  if (grid == nullptr) {
    return;
  }
  if (grid->cells != nullptr) {
    for (std::size_t index = 0; index < grid->count; ++index) {
      delete[] grid->cells[index].date;
      grid->cells[index].date = nullptr;
    }
    delete[] grid->cells;
  }
  grid->cells = nullptr;
  grid->count = 0;
  grid->max_secs = 0;
  grid->weeks = 0;
  grid->days_per_week = 0;
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

std::string derive_file_extension_from_path(std::string_view path) {
  const std::size_t slash = path.find_last_of("/\\");
  const std::size_t name_start = slash == std::string_view::npos ? 0 : slash + 1;
  if (name_start >= path.size()) {
    return {};
  }

  const std::string_view file_name = path.substr(name_start);
  const std::size_t dot = file_name.find_last_of('.');
  if (dot == std::string_view::npos || dot == 0 || dot + 1 >= file_name.size()) {
    return {};
  }
  return to_lower_ascii(file_name.substr(dot + 1));
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

std::uint32_t float_to_u32(const float value) {
  std::uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

float u32_to_float(const std::uint32_t bits) {
  float value = 0.0F;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

void write_u32_le(const std::uint32_t value, char* out) {
  out[0] = static_cast<char>(value & 0xFFu);
  out[1] = static_cast<char>((value >> 8u) & 0xFFu);
  out[2] = static_cast<char>((value >> 16u) & 0xFFu);
  out[3] = static_cast<char>((value >> 24u) & 0xFFu);
}

std::uint32_t read_u32_le(const std::uint8_t* in) {
  return static_cast<std::uint32_t>(in[0]) |
         (static_cast<std::uint32_t>(in[1]) << 8u) |
         (static_cast<std::uint32_t>(in[2]) << 16u) |
         (static_cast<std::uint32_t>(in[3]) << 24u);
}

kernel_status write_size_limit(std::size_t value, std::size_t* out_value) {
  if (out_value == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  *out_value = value;
  return kernel::core::make_status(KERNEL_OK);
}

kernel_status write_float_value(float value, float* out_value) {
  if (out_value == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  *out_value = value;
  return kernel::core::make_status(KERNEL_OK);
}

const char* truth_award_reason_key(const kernel_truth_award_reason reason) {
  switch (reason) {
    case KERNEL_TRUTH_AWARD_REASON_TEXT_DELTA:
      return "textDelta";
    case KERNEL_TRUTH_AWARD_REASON_CODE_LANGUAGE:
      return "codeLanguage";
    case KERNEL_TRUTH_AWARD_REASON_MOLECULAR_EDIT:
      return "molecularEdit";
    default:
      return nullptr;
  }
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

std::int64_t count_contiguous_study_streak(
    const std::set<std::int64_t>& active_days,
    const std::int64_t today_bucket) {
  std::int64_t streak_days = 0;
  std::int64_t expected = today_bucket;
  while (active_days.contains(expected)) {
    ++streak_days;
    if (expected == std::numeric_limits<std::int64_t>::min()) {
      break;
    }
    --expected;
  }
  return streak_days;
}

std::int64_t floor_div(const std::int64_t value, const std::int64_t divisor) {
  std::int64_t quotient = value / divisor;
  const std::int64_t remainder = value % divisor;
  if (remainder != 0 && ((remainder < 0) != (divisor < 0))) {
    --quotient;
  }
  return quotient;
}

std::int64_t positive_mod(const std::int64_t value, const std::int64_t modulus) {
  const std::int64_t remainder = value % modulus;
  return remainder < 0 ? remainder + modulus : remainder;
}

bool subtract_days(
    const std::int64_t epoch_secs,
    const std::int64_t days,
    std::int64_t* out_epoch_secs) {
  if (out_epoch_secs == nullptr || days < 0) {
    return false;
  }
  if (days > std::numeric_limits<std::int64_t>::max() / kSecsPerDay) {
    return false;
  }

  const std::int64_t delta = days * kSecsPerDay;
  if (epoch_secs < std::numeric_limits<std::int64_t>::min() + delta) {
    return false;
  }
  *out_epoch_secs = epoch_secs - delta;
  return true;
}

std::string format_date_from_epoch_secs(const std::int64_t epoch_secs) {
  const std::int64_t days = floor_div(epoch_secs, kSecsPerDay);
  const std::int64_t z = days + 719468;
  const std::int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const std::uint64_t doe = static_cast<std::uint64_t>(z - era * 146097);
  const std::uint64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  std::int64_t year = static_cast<std::int64_t>(yoe) + era * 400;
  const std::uint64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const std::uint64_t mp = (5 * doy + 2) / 153;
  const std::uint64_t day = doy - (153 * mp + 2) / 5 + 1;
  const std::uint64_t month = mp < 10 ? mp + 3 : mp - 9;
  if (month <= 2) {
    ++year;
  }

  char buffer[16]{};
  std::snprintf(
      buffer,
      sizeof(buffer),
      "%04lld-%02llu-%02llu",
      static_cast<long long>(year),
      static_cast<unsigned long long>(month),
      static_cast<unsigned long long>(day));
  return std::string(buffer);
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

extern "C" kernel_status kernel_get_truth_award_reason_key(
    const kernel_truth_award_reason reason,
    const char** out_key) {
  if (out_key == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  *out_key = nullptr;

  const char* key = truth_award_reason_key(reason);
  if (key == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  *out_key = key;
  return kernel::core::make_status(KERNEL_OK);
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
  return write_size_limit(kernel::core::product::rag_context_per_note_char_limit(), out_chars);
}

extern "C" kernel_status kernel_get_embedding_text_char_limit(std::size_t* out_chars) {
  return write_size_limit(kernel::core::product::embedding_text_char_limit(), out_chars);
}

extern "C" kernel_status kernel_get_ai_chat_timeout_secs(std::size_t* out_secs) {
  return write_size_limit(kernel::core::product::ai_chat_timeout_secs(), out_secs);
}

extern "C" kernel_status kernel_get_ai_ponder_timeout_secs(std::size_t* out_secs) {
  return write_size_limit(kernel::core::product::ai_ponder_timeout_secs(), out_secs);
}

extern "C" kernel_status kernel_get_ai_embedding_request_timeout_secs(std::size_t* out_secs) {
  return write_size_limit(kernel::core::product::ai_embedding_request_timeout_secs(), out_secs);
}

extern "C" kernel_status kernel_get_ai_embedding_cache_limit(std::size_t* out_limit) {
  return write_size_limit(kernel::core::product::ai_embedding_cache_limit(), out_limit);
}

extern "C" kernel_status kernel_get_ai_embedding_concurrency_limit(std::size_t* out_limit) {
  return write_size_limit(kernel::core::product::ai_embedding_concurrency_limit(), out_limit);
}

extern "C" kernel_status kernel_get_ai_rag_top_note_limit(std::size_t* out_limit) {
  return write_size_limit(kernel::core::product::ai_rag_top_note_limit(), out_limit);
}

extern "C" kernel_status kernel_derive_file_extension_from_path(
    const char* path,
    const std::size_t path_size,
    kernel_owned_buffer* out_buffer) {
  if (out_buffer == nullptr || (path_size > 0 && path == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  const std::string_view path_view(path == nullptr ? "" : path, path_size);
  const std::string extension = derive_file_extension_from_path(path_view);
  if (!fill_owned_buffer(extension, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_derive_note_display_name_from_path(
    const char* path,
    const std::size_t path_size,
    kernel_owned_buffer* out_buffer) {
  if (out_buffer == nullptr || (path_size > 0 && path == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  const std::string_view path_view(path == nullptr ? "" : path, path_size);
  const std::string display_name(kernel::core::product::derive_note_display_name_from_path(path_view));
  if (!fill_owned_buffer(display_name, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_normalize_database_column_type(
    const char* column_type,
    const std::size_t column_type_size,
    kernel_owned_buffer* out_buffer) {
  if (out_buffer == nullptr || (column_type_size > 0 && column_type == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  const std::string_view column_type_view(
      column_type == nullptr ? "" : column_type,
      column_type_size);
  const std::string normalized =
      kernel::core::product::normalize_database_column_type(column_type_view);
  if (!fill_owned_buffer(normalized, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_normalize_database_json(
    const char* json,
    const std::size_t json_size,
    kernel_owned_buffer* out_buffer) {
  if (out_buffer == nullptr || (json_size > 0 && json == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  const std::string_view raw(json == nullptr ? "" : json, json_size);
  std::string normalized;
  if (!kernel::core::product::normalize_database_json(raw, normalized)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  if (!fill_owned_buffer(normalized, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_build_paper_compile_plan_json(
    const char* workspace,
    const std::size_t workspace_size,
    const char* template_name,
    const std::size_t template_name_size,
    const char* const* image_paths,
    const std::size_t* image_path_sizes,
    const std::size_t image_path_count,
    const char* csl_path,
    const std::size_t csl_path_size,
    const char* bibliography_path,
    const std::size_t bibliography_path_size,
    const char* resource_separator,
    const std::size_t resource_separator_size,
    kernel_owned_buffer* out_buffer) {
  if (
      out_buffer == nullptr || (workspace_size > 0 && workspace == nullptr) ||
      (template_name_size > 0 && template_name == nullptr) ||
      (image_path_count > 0 && (image_paths == nullptr || image_path_sizes == nullptr)) ||
      (csl_path_size > 0 && csl_path == nullptr) ||
      (bibliography_path_size > 0 && bibliography_path == nullptr) ||
      (resource_separator_size > 0 && resource_separator == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  for (std::size_t index = 0; index < image_path_count; ++index) {
    if (image_path_sizes[index] > 0 && image_paths[index] == nullptr) {
      return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
    }
  }

  const std::string_view workspace_view(workspace == nullptr ? "" : workspace, workspace_size);
  const std::string_view template_view(
      template_name == nullptr ? "" : template_name,
      template_name_size);
  const std::string_view csl_view(csl_path == nullptr ? "" : csl_path, csl_path_size);
  const std::string_view bibliography_view(
      bibliography_path == nullptr ? "" : bibliography_path,
      bibliography_path_size);
  const std::string_view separator_view(
      resource_separator == nullptr ? "" : resource_separator,
      resource_separator_size);
  const std::string plan = kernel::core::product::build_paper_compile_plan_json(
      workspace_view,
      template_view,
      image_paths,
      image_path_sizes,
      image_path_count,
      csl_view,
      bibliography_view,
      separator_view);
  if (!fill_owned_buffer(plan, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_get_default_paper_template(kernel_owned_buffer* out_buffer) {
  if (out_buffer == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  if (!fill_owned_buffer(kernel::core::product::default_paper_template(), out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_summarize_paper_compile_log_json(
    const char* log,
    const std::size_t log_size,
    const std::size_t log_char_limit,
    kernel_owned_buffer* out_buffer) {
  if (out_buffer == nullptr || (log_size > 0 && log == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  const std::string_view raw(log == nullptr ? "" : log, log_size);
  const std::string summary =
      kernel::core::product::build_paper_compile_log_summary_json(raw, log_char_limit);
  if (!fill_owned_buffer(summary, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_normalize_pubchem_query(
    const char* query,
    const std::size_t query_size,
    kernel_owned_buffer* out_buffer) {
  if (out_buffer == nullptr || (query_size > 0 && query == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  const std::string_view raw(query == nullptr ? "" : query, query_size);
  const std::string normalized = kernel::core::product::normalize_pubchem_query(raw);
  if (normalized.empty()) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  if (!fill_owned_buffer(normalized, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_build_pubchem_compound_info_json(
    const char* query,
    const std::size_t query_size,
    const char* formula,
    const std::size_t formula_size,
    const double molecular_weight,
    const std::uint8_t has_density,
    const double density,
    const std::size_t property_count,
    kernel_owned_buffer* out_buffer) {
  if (
      out_buffer == nullptr || (query_size > 0 && query == nullptr) ||
      (formula_size > 0 && formula == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  const std::string_view query_view(query == nullptr ? "" : query, query_size);
  const std::string_view formula_view(formula == nullptr ? "" : formula, formula_size);
  const std::string payload = kernel::core::product::build_pubchem_compound_info_payload_json(
      query_view,
      formula_view,
      molecular_weight,
      has_density != 0,
      density,
      property_count);
  if (!fill_owned_buffer(payload, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_normalize_ai_embedding_text(
    const char* text,
    const std::size_t text_size,
    kernel_owned_buffer* out_buffer) {
  if (out_buffer == nullptr || (text_size > 0 && text == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  const std::string_view raw(text == nullptr ? "" : text, text_size);
  const std::string normalized = kernel::core::product::normalize_ai_embedding_text(raw);
  if (!kernel::core::product::is_ai_embedding_text_indexable(normalized)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  if (!fill_owned_buffer(normalized, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_is_ai_embedding_text_indexable(
    const char* text,
    const std::size_t text_size,
    std::uint8_t* out_is_indexable) {
  if (out_is_indexable == nullptr || (text_size > 0 && text == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string_view raw(text == nullptr ? "" : text, text_size);
  const std::string normalized = kernel::core::product::normalize_ai_embedding_text(raw);
  *out_is_indexable =
      static_cast<std::uint8_t>(kernel::core::product::is_ai_embedding_text_indexable(normalized));
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_should_refresh_ai_embedding_note(
    const std::int64_t note_updated_at,
    const std::uint8_t has_existing_updated_at,
    const std::int64_t existing_updated_at,
    std::uint8_t* out_should_refresh) {
  if (out_should_refresh == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  *out_should_refresh = static_cast<std::uint8_t>(
      has_existing_updated_at == 0 || note_updated_at > existing_updated_at);
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_compute_ai_embedding_cache_key(
    const char* base_url,
    const std::size_t base_url_size,
    const char* model,
    const std::size_t model_size,
    const char* text,
    const std::size_t text_size,
    kernel_owned_buffer* out_buffer) {
  if (
      out_buffer == nullptr || (base_url_size > 0 && base_url == nullptr) ||
      (model_size > 0 && model == nullptr) || (text_size > 0 && text == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  const std::string_view base_url_view(base_url == nullptr ? "" : base_url, base_url_size);
  const std::string_view model_view(model == nullptr ? "" : model, model_size);
  const std::string_view text_view(text == nullptr ? "" : text, text_size);
  const std::string key = kernel::core::product::compute_ai_embedding_cache_key(
      base_url_view,
      model_view,
      text_view);
  if (!fill_owned_buffer(key, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_serialize_ai_embedding_blob(
    const float* values,
    const std::size_t value_count,
    kernel_owned_buffer* out_buffer) {
  static_assert(sizeof(float) == 4, "AI embedding blob codec requires 32-bit floats");
  if (out_buffer == nullptr || (value_count > 0 && values == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  if (value_count > (std::numeric_limits<std::size_t>::max)() / 4u) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::string blob(value_count * 4u, '\0');
  for (std::size_t index = 0; index < value_count; ++index) {
    write_u32_le(float_to_u32(values[index]), blob.data() + (index * 4u));
  }
  if (!fill_owned_buffer(blob, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_parse_ai_embedding_blob(
    const std::uint8_t* blob,
    const std::size_t blob_size,
    kernel_float_buffer* out_values) {
  static_assert(sizeof(float) == 4, "AI embedding blob codec requires 32-bit floats");
  if (out_values == nullptr || (blob_size > 0 && blob == nullptr) || blob_size % 4u != 0u) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_values->values = nullptr;
  out_values->count = 0;

  const std::size_t value_count = blob_size / 4u;
  if (value_count == 0) {
    return kernel::core::make_status(KERNEL_OK);
  }

  auto* values = new (std::nothrow) float[value_count];
  if (values == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  for (std::size_t index = 0; index < value_count; ++index) {
    values[index] = u32_to_float(read_u32_le(blob + (index * 4u)));
  }

  out_values->values = values;
  out_values->count = value_count;
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" void kernel_free_float_buffer(kernel_float_buffer* buffer) {
  if (buffer == nullptr) {
    return;
  }
  delete[] buffer->values;
  buffer->values = nullptr;
  buffer->count = 0;
}

extern "C" kernel_status kernel_build_ai_rag_context(
    const char* const* note_names,
    const std::size_t* note_name_sizes,
    const char* const* note_contents,
    const std::size_t* note_content_sizes,
    const std::size_t note_count,
    kernel_owned_buffer* out_buffer) {
  if (
      out_buffer == nullptr ||
      (note_count > 0 &&
       (note_names == nullptr || note_name_sizes == nullptr || note_contents == nullptr ||
        note_content_sizes == nullptr))) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  for (std::size_t index = 0; index < note_count; ++index) {
    if (
        (note_name_sizes[index] > 0 && note_names[index] == nullptr) ||
        (note_content_sizes[index] > 0 && note_contents[index] == nullptr)) {
      return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
    }
  }

  const std::string context = kernel::core::product::build_ai_rag_context(
      note_names,
      note_name_sizes,
      note_contents,
      note_content_sizes,
      note_count);
  if (!fill_owned_buffer(context, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_build_ai_rag_context_from_note_paths(
    const char* const* note_paths,
    const std::size_t* note_path_sizes,
    const char* const* note_contents,
    const std::size_t* note_content_sizes,
    const std::size_t note_count,
    kernel_owned_buffer* out_buffer) {
  if (
      out_buffer == nullptr ||
      (note_count > 0 &&
       (note_paths == nullptr || note_path_sizes == nullptr || note_contents == nullptr ||
        note_content_sizes == nullptr))) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  for (std::size_t index = 0; index < note_count; ++index) {
    if (
        (note_path_sizes[index] > 0 && note_paths[index] == nullptr) ||
        (note_content_sizes[index] > 0 && note_contents[index] == nullptr)) {
      return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
    }
  }

  const std::string context = kernel::core::product::build_ai_rag_context_from_note_paths(
      note_paths,
      note_path_sizes,
      note_contents,
      note_content_sizes,
      note_count);
  if (!fill_owned_buffer(context, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_build_ai_rag_system_content(
    const char* context,
    const std::size_t context_size,
    kernel_owned_buffer* out_buffer) {
  if (out_buffer == nullptr || (context_size > 0 && context == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  const std::string_view context_view(context == nullptr ? "" : context, context_size);
  const std::string content = kernel::core::product::build_ai_rag_system_content(context_view);
  if (!fill_owned_buffer(content, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_get_ai_ponder_system_prompt(kernel_owned_buffer* out_buffer) {
  if (out_buffer == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  if (!fill_owned_buffer(kernel::core::product::ai_ponder_system_prompt(), out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_build_ai_ponder_user_prompt(
    const char* topic,
    const std::size_t topic_size,
    const char* context,
    const std::size_t context_size,
    kernel_owned_buffer* out_buffer) {
  if (
      out_buffer == nullptr || (topic_size > 0 && topic == nullptr) ||
      (context_size > 0 && context == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  const std::string_view topic_view(topic == nullptr ? "" : topic, topic_size);
  const std::string_view context_view(context == nullptr ? "" : context, context_size);
  const std::string prompt =
      kernel::core::product::build_ai_ponder_user_prompt(topic_view, context_view);
  if (!fill_owned_buffer(prompt, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_get_ai_ponder_temperature(float* out_temperature) {
  return write_float_value(kernel::core::product::ai_ponder_temperature(), out_temperature);
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

extern "C" kernel_status kernel_compute_study_stats_window(
    const std::int64_t now_epoch_secs,
    const std::int64_t days_back,
    kernel_study_stats_window* out_window) {
  if (out_window == nullptr || days_back <= 0) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_study_stats_window window{};
  window.today_start_epoch_secs = floor_div(now_epoch_secs, kSecsPerDay) * kSecsPerDay;
  window.today_bucket = floor_div(window.today_start_epoch_secs, kSecsPerDay);
  window.folder_rank_limit = kStudyFolderRankLimit;

  if (
      !subtract_days(
          window.today_start_epoch_secs,
          kStudyWeekLookbackDays,
          &window.week_start_epoch_secs) ||
      !subtract_days(
          window.today_start_epoch_secs,
          days_back - 1,
          &window.daily_window_start_epoch_secs) ||
      !subtract_days(
          window.today_start_epoch_secs,
          kStudyLegacyHeatmapLookbackDays,
          &window.heatmap_start_epoch_secs)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  *out_window = window;
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_compute_study_streak_days(
    const std::int64_t* day_buckets,
    const std::size_t day_count,
    const std::int64_t today_bucket,
    std::int64_t* out_streak_days) {
  if (out_streak_days == nullptr || (day_count > 0 && day_buckets == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  *out_streak_days = 0;

  std::set<std::int64_t> active_days;
  for (std::size_t index = 0; index < day_count; ++index) {
    active_days.insert(day_buckets[index]);
  }

  *out_streak_days = count_contiguous_study_streak(active_days, today_bucket);
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_compute_study_streak_days_from_timestamps(
    const std::int64_t* started_at_epoch_secs,
    const std::size_t timestamp_count,
    const std::int64_t today_bucket,
    std::int64_t* out_streak_days) {
  if (out_streak_days == nullptr || (timestamp_count > 0 && started_at_epoch_secs == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  *out_streak_days = 0;

  std::set<std::int64_t> active_days;
  for (std::size_t index = 0; index < timestamp_count; ++index) {
    active_days.insert(floor_div(started_at_epoch_secs[index], kSecsPerDay));
  }

  *out_streak_days = count_contiguous_study_streak(active_days, today_bucket);
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_build_study_heatmap_grid(
    const kernel_heatmap_day_activity* days,
    const std::size_t day_count,
    const std::int64_t now_epoch_secs,
    kernel_heatmap_grid* out_grid) {
  if (out_grid == nullptr || (day_count > 0 && days == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  reset_heatmap_grid_impl(out_grid);

  std::unordered_map<std::string, std::int64_t> secs_by_date;
  secs_by_date.reserve(day_count);
  for (std::size_t index = 0; index < day_count; ++index) {
    if (days[index].date == nullptr) {
      return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
    }
    secs_by_date[days[index].date] += days[index].active_secs;
  }

  constexpr std::size_t kTotalCells = kStudyHeatmapWeeks * kStudyHeatmapDaysPerWeek;
  out_grid->cells = new (std::nothrow) kernel_heatmap_cell[kTotalCells]{};
  if (out_grid->cells == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  out_grid->count = kTotalCells;
  out_grid->weeks = kStudyHeatmapWeeks;
  out_grid->days_per_week = kStudyHeatmapDaysPerWeek;

  const std::int64_t today_start = floor_div(now_epoch_secs, kSecsPerDay) * kSecsPerDay;
  const std::int64_t total_days = static_cast<std::int64_t>(kTotalCells);
  std::int64_t start_date = today_start - (total_days - 1) * kSecsPerDay;
  const std::int64_t day_of_week =
      positive_mod(floor_div(start_date, kSecsPerDay) + 3, 7);
  start_date -= day_of_week * kSecsPerDay;

  for (std::size_t week = 0; week < kStudyHeatmapWeeks; ++week) {
    for (std::size_t day = 0; day < kStudyHeatmapDaysPerWeek; ++day) {
      const std::size_t cell_index = week * kStudyHeatmapDaysPerWeek + day;
      const std::int64_t ts =
          start_date + static_cast<std::int64_t>(cell_index) * kSecsPerDay;
      const std::string date = format_date_from_epoch_secs(ts);
      const auto found = secs_by_date.find(date);
      const std::int64_t secs = found == secs_by_date.end() ? 0 : found->second;

      kernel_heatmap_cell& cell = out_grid->cells[cell_index];
      cell.date = kernel::core::duplicate_c_string(date);
      if (cell.date == nullptr) {
        reset_heatmap_grid_impl(out_grid);
        return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
      }
      cell.secs = secs;
      cell.col = week;
      cell.row = day;
      if (secs > out_grid->max_secs) {
        out_grid->max_secs = secs;
      }
    }
  }

  return kernel::core::make_status(KERNEL_OK);
}

extern "C" void kernel_free_study_heatmap_grid(kernel_heatmap_grid* grid) {
  reset_heatmap_grid_impl(grid);
}
