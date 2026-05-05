// Reason: Own paper compile planning and log summarization away from the
// product public ABI wrapper.

#include "core/kernel_product_paper.h"

#include "core/kernel_shared.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::size_t kPaperCompileHighlightLimit = 12;
constexpr char kDefaultPaperTemplate[] = "standard-thesis";

bool is_ascii_space(const char ch) {
  const auto byte = static_cast<unsigned char>(ch);
  return std::isspace(byte) != 0;
}

std::string_view trim_ascii(std::string_view value) {
  while (!value.empty() && is_ascii_space(value.front())) {
    value.remove_prefix(1);
  }
  while (!value.empty() && is_ascii_space(value.back())) {
    value.remove_suffix(1);
  }
  return value;
}

std::string to_lower_ascii(std::string_view value) {
  std::string lowered;
  lowered.reserve(value.size());
  for (const char ch : value) {
    lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
  }
  return lowered;
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

std::string json_string(std::string_view value) {
  return "\"" + kernel::core::json_escape(value) + "\"";
}

std::string parent_path_string(std::string_view path) {
  const std::size_t slash = path.find_last_of("/\\");
  if (slash == std::string_view::npos) {
    return {};
  }
  return std::string(path.substr(0, slash));
}

void append_template_args_json(std::string& output, std::string_view template_name) {
  const std::string normalized = to_lower_ascii(trim_ascii(template_name));
  if (normalized == "acs") {
    output += "\"-V\",\"papersize=letter\",\"-V\",\"fontsize=10pt\",\"-V\",\"geometry:margin=1in\"";
    return;
  }
  if (normalized == "nature") {
    output += "\"-V\",\"papersize=a4\",\"-V\",\"fontsize=10pt\",\"-V\",\"geometry:margin=1in\"";
    return;
  }
  if (normalized == "standard-thesis") {
    output += "\"-V\",\"documentclass=report\",\"-V\",\"fontsize=12pt\",\"-V\",\"geometry:margin=1in\"";
    return;
  }
  output += "\"-V\",\"documentclass=article\",\"-V\",\"fontsize=11pt\"";
}

bool is_paper_compile_highlight_line(std::string_view line) {
  const std::string lower = to_lower_ascii(line);
  return (
      lower.find("! ") != std::string::npos ||
      lower.find("error") != std::string::npos ||
      lower.find("undefined control sequence") != std::string::npos ||
      lower.find("missing") != std::string::npos ||
      lower.find("emergency stop") != std::string::npos ||
      lower.find("fatal") != std::string::npos);
}

std::string summarize_paper_compile_highlights(std::string_view log) {
  std::string summary;
  std::size_t start = 0;
  std::size_t count = 0;
  while (start <= log.size()) {
    const std::size_t end = log.find('\n', start);
    std::string_view line =
        end == std::string_view::npos ? log.substr(start) : log.substr(start, end - start);
    if (!line.empty() && line.back() == '\r') {
      line.remove_suffix(1);
    }

    const std::string_view trimmed = trim_ascii(line);
    if (!trimmed.empty() && is_paper_compile_highlight_line(trimmed)) {
      if (!summary.empty()) {
        summary.push_back('\n');
      }
      summary.append(trimmed);
      ++count;
      if (count >= kPaperCompileHighlightLimit) {
        break;
      }
    }

    if (end == std::string_view::npos) {
      break;
    }
    start = end + 1;
  }
  return summary;
}

std::string build_resource_path(
    std::string_view workspace,
    const char* const* image_paths,
    const std::size_t* image_path_sizes,
    const std::size_t image_path_count,
    std::string_view separator) {
  std::vector<std::string> roots;
  if (!workspace.empty()) {
    roots.push_back(std::string(workspace));
  }

  for (std::size_t index = 0; index < image_path_count; ++index) {
    const std::string_view image_path(image_paths[index], image_path_sizes[index]);
    const std::string parent = parent_path_string(image_path);
    if (
        trim_ascii(parent).empty() ||
        std::find(roots.begin(), roots.end(), parent) != roots.end()) {
      continue;
    }
    roots.push_back(parent);
  }

  std::string joined;
  for (std::size_t index = 0; index < roots.size(); ++index) {
    if (index != 0) {
      joined.append(separator);
    }
    joined.append(roots[index]);
  }
  return joined;
}

}  // namespace

namespace kernel::core::product {

std::string_view default_paper_template() {
  return kDefaultPaperTemplate;
}

std::string build_paper_compile_plan_json(
    std::string_view workspace,
    std::string_view template_name,
    const char* const* image_paths,
    const std::size_t* image_path_sizes,
    const std::size_t image_path_count,
    std::string_view csl_path,
    std::string_view bibliography_path,
    std::string_view resource_separator) {
  std::string output = "{\"templateArgs\":[";
  append_template_args_json(output, template_name);
  output += "],\"cslPath\":";
  output += json_string(trim_ascii(csl_path));
  output += ",\"bibliographyPath\":";
  output += json_string(trim_ascii(bibliography_path));
  output += ",\"resourcePath\":";
  output += json_string(build_resource_path(
      workspace,
      image_paths,
      image_path_sizes,
      image_path_count,
      resource_separator.empty() ? std::string_view(";") : resource_separator));
  output += "}";
  return output;
}

std::string build_paper_compile_log_summary_json(
    std::string_view log,
    const std::size_t log_char_limit) {
  const std::size_t prefix_size = utf8_prefix_bytes_by_chars(log, log_char_limit);
  const bool truncated = prefix_size < log.size();

  std::string output = "{\"summary\":";
  output += json_string(summarize_paper_compile_highlights(log));
  output += ",\"logPrefix\":";
  output += json_string(log.substr(0, prefix_size));
  output += ",\"truncated\":";
  output += truncated ? "true" : "false";
  output += "}";
  return output;
}

}  // namespace kernel::core::product
