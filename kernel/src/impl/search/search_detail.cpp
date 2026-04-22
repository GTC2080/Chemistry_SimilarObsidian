// Reason: This file keeps shared token, snippet, and SQLite binding helpers out of the public search orchestration unit.

#include "search/search_detail.h"

#include <algorithm>
#include <cctype>
#include <limits>

namespace {

std::string escape_fts_phrase(std::string_view token) {
  std::string escaped;
  escaped.reserve(token.size() + 2);
  escaped.push_back('"');
  for (const char ch : token) {
    if (ch == '"') {
      escaped.push_back('"');
    }
    escaped.push_back(ch);
  }
  escaped.push_back('"');
  return escaped;
}

std::string lowercase_ascii(std::string_view text) {
  std::string lowered;
  lowered.reserve(text.size());
  for (const char ch : text) {
    lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
  }
  return lowered;
}

std::string escape_like_pattern(std::string_view text) {
  std::string escaped;
  escaped.reserve(text.size() + 8);
  for (const char ch : text) {
    if (ch == '\\' || ch == '%' || ch == '_') {
      escaped.push_back('\\');
    }
    escaped.push_back(ch);
  }
  return escaped;
}

std::string_view trim_ascii_whitespace(std::string_view text) {
  std::size_t begin = 0;
  std::size_t end = text.size();
  while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) {
    ++begin;
  }
  while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
    --end;
  }
  return text.substr(begin, end - begin);
}

std::string collapse_ascii_whitespace(std::string_view text) {
  std::string collapsed;
  collapsed.reserve(text.size());

  bool pending_space = false;
  for (const char ch : text) {
    if (std::isspace(static_cast<unsigned char>(ch))) {
      pending_space = !collapsed.empty();
      continue;
    }

    if (pending_space) {
      collapsed.push_back(' ');
      pending_space = false;
    }
    collapsed.push_back(ch);
  }

  return collapsed;
}

std::size_t find_first_token_match_case_insensitive(
    std::string_view haystack,
    const std::vector<std::string>& tokens) {
  const std::string lowered_haystack = lowercase_ascii(haystack);
  std::size_t earliest = std::string::npos;
  for (const auto& token : tokens) {
    const std::size_t candidate = lowered_haystack.find(lowercase_ascii(token));
    if (candidate != std::string::npos &&
        (earliest == std::string::npos || candidate < earliest)) {
      earliest = candidate;
    }
  }
  return earliest;
}

bool is_utf8_continuation_byte(const unsigned char byte) {
  return (byte & 0xc0u) == 0x80u;
}

std::size_t align_utf8_start(std::string_view text, std::size_t offset) {
  while (offset < text.size() &&
         is_utf8_continuation_byte(static_cast<unsigned char>(text[offset]))) {
    ++offset;
  }
  return offset;
}

std::size_t align_utf8_end(std::string_view text, std::size_t offset) {
  if (offset >= text.size()) {
    return text.size();
  }

  while (offset > 0 &&
         is_utf8_continuation_byte(static_cast<unsigned char>(text[offset]))) {
    --offset;
  }
  return offset;
}

}  // namespace

namespace kernel::search::detail {

bool has_field(std::string_view value) {
  return !value.empty();
}

std::error_code make_sqlite_error(const int rc) {
  return std::error_code(rc, std::generic_category());
}

std::error_code bind_text_param(
    sqlite3_stmt* stmt,
    const int index,
    std::string_view text) {
  const char* data = text.empty() ? "" : text.data();
  const int rc = sqlite3_bind_text(
      stmt,
      index,
      data,
      static_cast<int>(text.size()),
      SQLITE_TRANSIENT);
  return rc == SQLITE_OK ? std::error_code{} : make_sqlite_error(rc);
}

std::error_code bind_int64_param(
    sqlite3_stmt* stmt,
    const int index,
    const sqlite3_int64 value) {
  const int rc = sqlite3_bind_int64(stmt, index, value);
  return rc == SQLITE_OK ? std::error_code{} : make_sqlite_error(rc);
}

std::vector<std::string> split_literal_tokens(std::string_view query) {
  std::vector<std::string> tokens;
  std::size_t cursor = 0;

  while (cursor < query.size()) {
    while (cursor < query.size() && std::isspace(static_cast<unsigned char>(query[cursor]))) {
      ++cursor;
    }
    if (cursor >= query.size()) {
      break;
    }

    const std::size_t token_begin = cursor;
    while (cursor < query.size() && !std::isspace(static_cast<unsigned char>(query[cursor]))) {
      ++cursor;
    }

    tokens.emplace_back(query.substr(token_begin, cursor - token_begin));
  }

  return tokens;
}

std::string build_literal_match_query(const std::vector<std::string>& tokens) {
  std::string result;
  bool first = true;
  for (const auto& token : tokens) {
    if (!first) {
      result.push_back(' ');
    }
    result += escape_fts_phrase(token);
    first = false;
  }
  return result;
}

std::string build_prefix_like_pattern(std::string_view prefix) {
  std::string pattern = escape_like_pattern(prefix);
  pattern.push_back('%');
  return pattern;
}

std::string build_contains_like_pattern(std::string_view token) {
  std::string pattern = "%";
  pattern += escape_like_pattern(lowercase_ascii(token));
  pattern.push_back('%');
  return pattern;
}

bool contains_all_tokens_case_insensitive(
    std::string_view haystack,
    const std::vector<std::string>& tokens) {
  const std::string lowered_haystack = lowercase_ascii(haystack);
  for (const auto& token : tokens) {
    if (lowered_haystack.find(lowercase_ascii(token)) == std::string::npos) {
      return false;
    }
  }
  return !tokens.empty();
}

std::string_view strip_leading_title_heading(
    std::string_view body_text,
    std::string_view title) {
  const std::size_t newline = body_text.find('\n');
  const std::string_view first_line = body_text.substr(0, newline);
  const std::string_view trimmed_line = trim_ascii_whitespace(first_line);

  std::size_t cursor = 0;
  while (cursor < trimmed_line.size() && trimmed_line[cursor] == '#') {
    ++cursor;
  }
  if (cursor == 0 || cursor > 6 || cursor >= trimmed_line.size() ||
      !std::isspace(static_cast<unsigned char>(trimmed_line[cursor]))) {
    return body_text;
  }

  const std::string_view heading_text = trim_ascii_whitespace(trimmed_line.substr(cursor));
  if (heading_text != title) {
    return body_text;
  }

  if (newline == std::string_view::npos) {
    return {};
  }
  return body_text.substr(newline + 1);
}

sqlite3_int64 clamp_size_for_sqlite(const std::size_t value) {
  constexpr auto kSqliteMax =
      static_cast<std::size_t>(std::numeric_limits<sqlite3_int64>::max());
  if (value > kSqliteMax) {
    return std::numeric_limits<sqlite3_int64>::max();
  }
  return static_cast<sqlite3_int64>(value);
}

std::string build_body_snippet(
    std::string_view body_text,
    const std::vector<std::string>& tokens) {
  const std::string collapsed = collapse_ascii_whitespace(body_text);
  if (collapsed.empty()) {
    return {};
  }

  if (collapsed.size() <= kSearchSnippetMaxBytes) {
    return collapsed;
  }

  const std::size_t first_match =
      find_first_token_match_case_insensitive(collapsed, tokens);
  if (first_match == std::string::npos) {
    const std::size_t end =
        align_utf8_end(collapsed, std::min(collapsed.size(), kSearchSnippetMaxBytes));
    return std::string(collapsed.substr(0, end));
  }

  const std::size_t context_before = kSearchSnippetMaxBytes / 4;
  std::size_t start = first_match > context_before ? first_match - context_before : 0;
  start = align_utf8_start(collapsed, start);

  std::size_t end = start + kSearchSnippetMaxBytes;
  if (end > collapsed.size()) {
    end = collapsed.size();
  } else {
    end = align_utf8_end(collapsed, end);
  }

  if (end <= start) {
    start = 0;
    end = align_utf8_end(collapsed, std::min(collapsed.size(), kSearchSnippetMaxBytes));
  }
  return std::string(collapsed.substr(start, end - start));
}

std::string build_rank_tag_exact_token(
    const SearchQuery& query,
    const std::vector<std::string>& tokens) {
  if (query.sort_mode != KERNEL_SEARCH_SORT_RANK_V1 || tokens.size() != 1) {
    return {};
  }
  return tokens.front();
}

}  // namespace kernel::search::detail
