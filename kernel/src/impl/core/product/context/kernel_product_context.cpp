// Reason: Own semantic context and product path rules away from the public ABI wrapper.

#include "core/kernel_product_context.h"

#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::size_t kMinContextChars = 24;
constexpr std::size_t kMaxContextChars = 2200;

bool is_ascii_space(const char ch) {
  const auto byte = static_cast<unsigned char>(ch);
  return std::isspace(byte) != 0;
}

std::string to_lower_ascii(std::string_view value) {
  std::string lowered;
  lowered.reserve(value.size());
  for (const char ch : value) {
    lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
  }
  return lowered;
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

}  // namespace

namespace kernel::core::product {

std::size_t semantic_context_min_bytes() {
  return kMinContextChars;
}

std::string build_semantic_context(std::string_view content) {
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

}  // namespace kernel::core::product
