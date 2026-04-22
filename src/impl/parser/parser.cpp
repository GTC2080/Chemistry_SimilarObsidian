// Reason: This file implements the minimum markdown scanning rules frozen for the first parser skeleton.

#include "parser/parser.h"

#include <cctype>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

namespace kernel::parser {
namespace {

std::string_view trim_ascii_whitespace(std::string_view value) {
  std::size_t start = 0;
  while (start < value.size()) {
    const unsigned char ch = static_cast<unsigned char>(value[start]);
    if (!std::isspace(ch)) {
      break;
    }
    ++start;
  }

  std::size_t end = value.size();
  while (end > start) {
    const unsigned char ch = static_cast<unsigned char>(value[end - 1]);
    if (!std::isspace(ch)) {
      break;
    }
    --end;
  }

  return value.substr(start, end - start);
}

bool is_tag_start(const std::string_view markdown, const std::size_t index) {
  if (markdown[index] != '#') {
    return false;
  }
  if (index + 1 >= markdown.size()) {
    return false;
  }

  const unsigned char next = static_cast<unsigned char>(markdown[index + 1]);
  if (!(std::isalnum(next) || next == '_')) {
    return false;
  }

  if (index == 0) {
    return true;
  }

  const unsigned char previous = static_cast<unsigned char>(markdown[index - 1]);
  return !(std::isalnum(previous) || previous == '_' || previous == '#');
}

bool is_tag_char(const char ch) {
  const unsigned char value = static_cast<unsigned char>(ch);
  return std::isalnum(value) || value == '_';
}

bool looks_like_local_attachment_target(std::string_view target) {
  target = trim_ascii_whitespace(target);
  if (target.empty() || target[0] == '#') {
    return false;
  }

  if (target.size() >= 2 && target.front() == '<' && target.back() == '>') {
    target.remove_prefix(1);
    target.remove_suffix(1);
    target = trim_ascii_whitespace(target);
  }

  if (target.find("://") != std::string_view::npos) {
    return false;
  }

  const std::filesystem::path path{std::string(target)};
  std::string extension = path.extension().string();
  for (char& ch : extension) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }

  return !extension.empty() && extension != ".md";
}

std::string normalized_local_attachment_target(std::string_view target) {
  target = trim_ascii_whitespace(target);
  if (target.empty() || target[0] == '#') {
    return {};
  }

  if (target.size() >= 2 && target.front() == '<' && target.back() == '>') {
    target.remove_prefix(1);
    target.remove_suffix(1);
    target = trim_ascii_whitespace(target);
  }
  if (target.find("://") != std::string_view::npos) {
    return {};
  }

  const std::size_t anchor_fragment = target.find("#anchor=");
  if (anchor_fragment != std::string_view::npos) {
    target = trim_ascii_whitespace(target.substr(0, anchor_fragment));
  }

  const std::filesystem::path path{std::string(target)};
  std::string extension = path.extension().string();
  for (char& ch : extension) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }

  if (extension.empty() || extension == ".md") {
    return {};
  }
  return std::string(target);
}

bool extract_pdf_source_ref_target(
    std::string_view target,
    PdfSourceRef& out_source_ref) {
  out_source_ref = PdfSourceRef{};
  target = trim_ascii_whitespace(target);
  if (target.empty()) {
    return false;
  }

  if (target.size() >= 2 && target.front() == '<' && target.back() == '>') {
    target.remove_prefix(1);
    target.remove_suffix(1);
    target = trim_ascii_whitespace(target);
  }
  if (target.find("://") != std::string_view::npos) {
    return false;
  }

  const std::size_t anchor_fragment = target.find("#anchor=");
  if (anchor_fragment == std::string_view::npos) {
    return false;
  }

  const std::string normalized_attachment =
      normalized_local_attachment_target(target);
  if (normalized_attachment.empty()) {
    return false;
  }

  std::filesystem::path path{normalized_attachment};
  std::string extension = path.extension().string();
  for (char& ch : extension) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  if (extension != ".pdf") {
    return false;
  }

  const std::string_view anchor_serialized =
      trim_ascii_whitespace(target.substr(anchor_fragment + 8));
  if (anchor_serialized.empty()) {
    return false;
  }

  out_source_ref.pdf_rel_path = normalized_attachment;
  out_source_ref.anchor_serialized = std::string(anchor_serialized);
  return true;
}

void parse_title_line(std::string_view line, ParseResult& result) {
  if (!result.title.empty() || line.empty() || line[0] != '#') {
    return;
  }

  std::size_t index = 0;
  while (index < line.size() && line[index] == '#') {
    ++index;
  }

  if (index == 0 || index >= line.size() || line[index] != ' ') {
    return;
  }

  while (index < line.size() && line[index] == ' ') {
    ++index;
  }
  if (index >= line.size()) {
    return;
  }

  const std::string_view title = trim_ascii_whitespace(line.substr(index));
  if (!title.empty()) {
    result.title.assign(title);
  }
}

void scan_tags(std::string_view markdown, ParseResult& result) {
  for (std::size_t index = 0; index < markdown.size(); ++index) {
    if (!is_tag_start(markdown, index)) {
      continue;
    }

    std::size_t cursor = index + 1;
    while (cursor < markdown.size() && is_tag_char(markdown[cursor])) {
      ++cursor;
    }

    result.tags.emplace_back(markdown.substr(index + 1, cursor - index - 1));
    index = cursor - 1;
  }
}

void scan_wikilinks(std::string_view markdown, ParseResult& result) {
  std::size_t cursor = 0;
  while (cursor + 3 < markdown.size()) {
    const std::size_t open = markdown.find("[[", cursor);
    if (open == std::string_view::npos) {
      return;
    }
    if (open > 0 && markdown[open - 1] == '!') {
      cursor = open + 2;
      continue;
    }

    const std::size_t close = markdown.find("]]", open + 2);
    if (close == std::string_view::npos) {
      return;
    }

    std::string_view target = markdown.substr(open + 2, close - open - 2);
    const std::size_t alias = target.find('|');
    if (alias != std::string_view::npos) {
      target = target.substr(0, alias);
    }
    target = trim_ascii_whitespace(target);
    if (!target.empty()) {
      result.wikilinks.emplace_back(target);
    }

    cursor = close + 2;
  }
}

void scan_markdown_attachment_links(std::string_view markdown, ParseResult& result) {
  std::size_t cursor = 0;
  while (cursor + 4 < markdown.size()) {
    const std::size_t close_bracket = markdown.find(']', cursor);
    if (close_bracket == std::string_view::npos || close_bracket + 1 >= markdown.size()) {
      return;
    }
    if (markdown[close_bracket + 1] != '(') {
      cursor = close_bracket + 1;
      continue;
    }

    const std::size_t close_paren = markdown.find(')', close_bracket + 2);
    if (close_paren == std::string_view::npos) {
      return;
    }

    std::string_view target = trim_ascii_whitespace(
        markdown.substr(close_bracket + 2, close_paren - close_bracket - 2));
    const std::string normalized_attachment =
        normalized_local_attachment_target(target);
    if (!normalized_attachment.empty()) {
      result.attachment_refs.push_back(normalized_attachment);
    }

    PdfSourceRef source_ref;
    if (extract_pdf_source_ref_target(target, source_ref)) {
      result.pdf_source_refs.push_back(std::move(source_ref));
    }

    cursor = close_paren + 1;
  }
}

void scan_embedded_attachment_refs(std::string_view markdown, ParseResult& result) {
  std::size_t cursor = 0;
  while (cursor + 4 < markdown.size()) {
    const std::size_t open = markdown.find("![[", cursor);
    if (open == std::string_view::npos) {
      return;
    }

    const std::size_t close = markdown.find("]]", open + 3);
    if (close == std::string_view::npos) {
      return;
    }

    std::string_view target = markdown.substr(open + 3, close - open - 3);
    const std::size_t alias = target.find('|');
    if (alias != std::string_view::npos) {
      target = target.substr(0, alias);
    }
    target = trim_ascii_whitespace(target);
    const std::string normalized_attachment =
        normalized_local_attachment_target(target);
    if (!normalized_attachment.empty()) {
      result.attachment_refs.push_back(normalized_attachment);
    }

    cursor = close + 2;
  }
}

}  // namespace

ParseResult parse_markdown(std::string_view markdown) {
  ParseResult result;

  std::size_t line_start = 0;
  while (line_start <= markdown.size()) {
    const std::size_t line_end = markdown.find('\n', line_start);
    const std::size_t length =
        line_end == std::string_view::npos ? markdown.size() - line_start : line_end - line_start;
    parse_title_line(markdown.substr(line_start, length), result);

    if (line_end == std::string_view::npos) {
      break;
    }
    line_start = line_end + 1;
  }

  scan_tags(markdown, result);
  scan_wikilinks(markdown, result);
  scan_markdown_attachment_links(markdown, result);
  scan_embedded_attachment_refs(markdown, result);
  return result;
}

}  // namespace kernel::parser
