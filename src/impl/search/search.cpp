// Reason: This file implements the host-facing search query surface over the local SQLite state store.

#include "search/search.h"

#include "third_party/sqlite/sqlite3.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <limits>
#include <vector>

namespace kernel::search {
namespace {

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

std::string_view strip_leading_title_heading(std::string_view body_text, std::string_view title) {
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

  const std::size_t first_match = find_first_token_match_case_insensitive(collapsed, tokens);
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

std::string build_note_count_sql(const SearchQuery& query) {
  std::string sql =
      "SELECT COUNT(DISTINCT notes.note_id) "
      "FROM note_fts "
      "JOIN notes ON notes.note_id = note_fts.rowid ";
  if (has_field(query.tag_filter)) {
    sql += "JOIN note_tags ON note_tags.note_id = notes.note_id ";
  }
  sql += "WHERE note_fts MATCH ? AND notes.is_deleted = 0 ";
  if (has_field(query.tag_filter)) {
    sql += "AND note_tags.tag = ? ";
  }
  if (has_field(query.path_prefix)) {
    sql += "AND notes.rel_path LIKE ? ESCAPE '\\' ";
  }
  sql.push_back(';');
  return sql;
}

std::string build_note_select_sql(const SearchQuery& query) {
  std::string sql =
      "SELECT DISTINCT "
      "notes.rel_path, "
      "notes.title, "
      "note_fts.title, "
      "note_fts.body, "
      "bm25(note_fts) AS raw_fts_score, "
      "CASE "
      "  WHEN ? = '' THEN 0 "
      "  WHEN EXISTS("
      "    SELECT 1 FROM note_tags AS boost_tags "
      "    WHERE boost_tags.note_id = notes.note_id AND boost_tags.tag = ?"
      "  ) THEN 1 "
      "  ELSE 0 "
      "END AS tag_exact_hit "
      "FROM note_fts "
      "JOIN notes ON notes.note_id = note_fts.rowid ";
  if (has_field(query.tag_filter)) {
    sql += "JOIN note_tags ON note_tags.note_id = notes.note_id ";
  }
  sql += "WHERE note_fts MATCH ? AND notes.is_deleted = 0 ";
  if (has_field(query.tag_filter)) {
    sql += "AND note_tags.tag = ? ";
  }
  if (has_field(query.path_prefix)) {
    sql += "AND notes.rel_path LIKE ? ESCAPE '\\' ";
  }
  sql += "ORDER BY notes.rel_path ASC LIMIT ? OFFSET ?;";
  return sql;
}

std::error_code bind_note_query_params(
    sqlite3_stmt* stmt,
    const SearchQuery& query,
    const std::string& match_query,
    const std::string& rank_tag_exact_token,
    int& next_param_index) {
  std::error_code ec = bind_text_param(stmt, next_param_index++, rank_tag_exact_token);
  if (ec) {
    return ec;
  }
  ec = bind_text_param(stmt, next_param_index++, rank_tag_exact_token);
  if (ec) {
    return ec;
  }
  ec = bind_text_param(stmt, next_param_index++, match_query);
  if (ec) {
    return ec;
  }
  if (has_field(query.tag_filter)) {
    ec = bind_text_param(stmt, next_param_index++, query.tag_filter);
    if (ec) {
      return ec;
    }
  }
  if (has_field(query.path_prefix)) {
    const std::string pattern = build_prefix_like_pattern(query.path_prefix);
    ec = bind_text_param(stmt, next_param_index++, pattern);
    if (ec) {
      return ec;
    }
  }
  return {};
}

std::error_code count_note_hits(
    sqlite3* db,
    const SearchQuery& query,
    const std::string& match_query,
    std::uint64_t& out_total_hits) {
  sqlite3_stmt* stmt = nullptr;
  const std::string sql = build_note_count_sql(query);
  const int prepare_rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
  if (prepare_rc != SQLITE_OK) {
    return make_sqlite_error(prepare_rc);
  }

  int param_index = 1;
  std::error_code ec = bind_text_param(stmt, param_index++, match_query);
  if (ec) {
    sqlite3_finalize(stmt);
    return ec;
  }
  if (has_field(query.tag_filter)) {
    ec = bind_text_param(stmt, param_index++, query.tag_filter);
    if (ec) {
      sqlite3_finalize(stmt);
      return ec;
    }
  }
  if (has_field(query.path_prefix)) {
    const std::string pattern = build_prefix_like_pattern(query.path_prefix);
    ec = bind_text_param(stmt, param_index++, pattern);
    if (ec) {
      sqlite3_finalize(stmt);
      return ec;
    }
  }

  const int step_rc = sqlite3_step(stmt);
  if (step_rc != SQLITE_ROW) {
    sqlite3_finalize(stmt);
    return make_sqlite_error(step_rc);
  }

  out_total_hits = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 0));
  sqlite3_finalize(stmt);
  return {};
}

std::error_code append_note_hits(
    sqlite3* db,
    const SearchQuery& query,
    const std::string& match_query,
    const std::vector<std::string>& tokens,
    const std::string& rank_tag_exact_token,
    std::vector<SearchHit>& out_hits) {
  sqlite3_stmt* stmt = nullptr;
  const std::string sql = build_note_select_sql(query);
  const int prepare_rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
  if (prepare_rc != SQLITE_OK) {
    return make_sqlite_error(prepare_rc);
  }

  int param_index = 1;
  std::error_code ec =
      bind_note_query_params(stmt, query, match_query, rank_tag_exact_token, param_index);
  if (ec) {
    sqlite3_finalize(stmt);
    return ec;
  }
  ec = bind_int64_param(stmt, param_index++, clamp_size_for_sqlite(query.limit));
  if (ec) {
    sqlite3_finalize(stmt);
    return ec;
  }
  ec = bind_int64_param(stmt, param_index++, clamp_size_for_sqlite(query.offset));
  if (ec) {
    sqlite3_finalize(stmt);
    return ec;
  }

  while (true) {
    const int step_rc = sqlite3_step(stmt);
    if (step_rc == SQLITE_DONE) {
      break;
    }
    if (step_rc != SQLITE_ROW) {
      sqlite3_finalize(stmt);
      return make_sqlite_error(step_rc);
    }

    const unsigned char* rel_path = sqlite3_column_text(stmt, 0);
    const unsigned char* title = sqlite3_column_text(stmt, 1);
    const unsigned char* indexed_title = sqlite3_column_text(stmt, 2);
    const unsigned char* indexed_body = sqlite3_column_text(stmt, 3);

    SearchHit hit;
    if (rel_path != nullptr) {
      hit.rel_path = reinterpret_cast<const char*>(rel_path);
    }
    if (title != nullptr) {
      hit.title = reinterpret_cast<const char*>(title);
    }

    if (indexed_title != nullptr &&
        contains_all_tokens_case_insensitive(
            reinterpret_cast<const char*>(indexed_title),
            tokens)) {
      hit.match_flags |= KERNEL_SEARCH_MATCH_TITLE;
      hit.title_rank_hit = true;
    }

    const std::string_view body_without_title =
        indexed_body == nullptr
            ? std::string_view{}
            : strip_leading_title_heading(
                  reinterpret_cast<const char*>(indexed_body),
                  indexed_title == nullptr
                      ? std::string_view{}
                      : std::string_view(reinterpret_cast<const char*>(indexed_title)));
    if (!body_without_title.empty() &&
        contains_all_tokens_case_insensitive(body_without_title, tokens)) {
      hit.match_flags |= KERNEL_SEARCH_MATCH_BODY;
      hit.snippet = build_body_snippet(body_without_title, tokens);
      hit.snippet_status = hit.snippet.empty()
                               ? KERNEL_SEARCH_SNIPPET_UNAVAILABLE
                               : KERNEL_SEARCH_SNIPPET_BODY_EXTRACTED;
    } else if ((hit.match_flags & KERNEL_SEARCH_MATCH_TITLE) != 0) {
      hit.snippet_status = KERNEL_SEARCH_SNIPPET_TITLE_ONLY;
    } else {
      hit.snippet_status = KERNEL_SEARCH_SNIPPET_UNAVAILABLE;
    }

    hit.raw_fts_score = sqlite3_column_double(stmt, 4);
    hit.tag_exact_rank_hit = sqlite3_column_int(stmt, 5) != 0;
    hit.score = query.sort_mode == KERNEL_SEARCH_SORT_RANK_V1 ? -hit.raw_fts_score : 0.0;

    out_hits.push_back(std::move(hit));
  }

  sqlite3_finalize(stmt);
  return {};
}

bool ranked_note_before(const SearchHit& lhs, const SearchHit& rhs) {
  if (lhs.title_rank_hit != rhs.title_rank_hit) {
    return lhs.title_rank_hit && !rhs.title_rank_hit;
  }
  if (lhs.tag_exact_rank_hit != rhs.tag_exact_rank_hit) {
    return lhs.tag_exact_rank_hit && !rhs.tag_exact_rank_hit;
  }
  if (lhs.raw_fts_score != rhs.raw_fts_score) {
    return lhs.raw_fts_score < rhs.raw_fts_score;
  }
  return lhs.rel_path < rhs.rel_path;
}

void move_ranked_slice(
    std::vector<SearchHit>& source_hits,
    const std::uint64_t offset,
    const std::size_t limit,
    std::vector<SearchHit>& out_hits) {
  const std::size_t begin = static_cast<std::size_t>(
      std::min<std::uint64_t>(offset, source_hits.size()));
  const std::size_t end =
      std::min<std::size_t>(begin + limit, source_hits.size());
  out_hits.reserve(end - begin);
  for (std::size_t index = begin; index < end; ++index) {
    out_hits.push_back(std::move(source_hits[index]));
  }
}

std::error_code collect_ranked_note_hits(
    sqlite3* db,
    const SearchQuery& query,
    const std::string& match_query,
    const std::vector<std::string>& tokens,
    std::vector<SearchHit>& out_hits) {
  SearchQuery fetch_query = query;
  fetch_query.offset = 0;
  fetch_query.limit = static_cast<std::size_t>(-1);
  const std::string rank_tag_exact_token =
      build_rank_tag_exact_token(query, tokens);
  std::error_code ec = append_note_hits(
      db,
      fetch_query,
      match_query,
      tokens,
      rank_tag_exact_token,
      out_hits);
  if (ec) {
    return ec;
  }

  std::stable_sort(out_hits.begin(), out_hits.end(), ranked_note_before);
  return {};
}

std::string build_attachment_count_sql(
    const SearchQuery& query,
    const std::size_t token_count) {
  std::string sql =
      "SELECT COUNT(*) FROM ("
      "SELECT DISTINCT refs.attachment_rel_path "
      "FROM note_attachment_refs AS refs "
      "JOIN notes ON notes.note_id = refs.note_id AND notes.is_deleted = 0 ";
  if (has_field(query.tag_filter)) {
    sql += "JOIN note_tags ON note_tags.note_id = notes.note_id ";
  }
  sql += "WHERE 1=1 ";
  if (has_field(query.tag_filter)) {
    sql += "AND note_tags.tag = ? ";
  }
  if (has_field(query.path_prefix)) {
    sql += "AND refs.attachment_rel_path LIKE ? ESCAPE '\\' ";
  }
  for (std::size_t index = 0; index < token_count; ++index) {
    sql += "AND LOWER(refs.attachment_rel_path) LIKE ? ESCAPE '\\' ";
  }
  sql += ");";
  return sql;
}

std::string build_attachment_select_sql(
    const SearchQuery& query,
    const std::size_t token_count) {
  std::string sql =
      "SELECT refs.attachment_rel_path, COALESCE(MAX(attachments.is_missing), 0) "
      "FROM note_attachment_refs AS refs "
      "JOIN notes ON notes.note_id = refs.note_id AND notes.is_deleted = 0 ";
  if (has_field(query.tag_filter)) {
    sql += "JOIN note_tags ON note_tags.note_id = notes.note_id ";
  }
  sql +=
      "LEFT JOIN attachments ON attachments.rel_path = refs.attachment_rel_path "
      "WHERE 1=1 ";
  if (has_field(query.tag_filter)) {
    sql += "AND note_tags.tag = ? ";
  }
  if (has_field(query.path_prefix)) {
    sql += "AND refs.attachment_rel_path LIKE ? ESCAPE '\\' ";
  }
  for (std::size_t index = 0; index < token_count; ++index) {
    sql += "AND LOWER(refs.attachment_rel_path) LIKE ? ESCAPE '\\' ";
  }
  sql +=
      "GROUP BY refs.attachment_rel_path "
      "ORDER BY refs.attachment_rel_path ASC "
      "LIMIT ? OFFSET ?;";
  return sql;
}

std::error_code bind_attachment_query_params(
    sqlite3_stmt* stmt,
    const SearchQuery& query,
    const std::vector<std::string>& tokens,
    int& next_param_index) {
  if (has_field(query.tag_filter)) {
    std::error_code ec = bind_text_param(stmt, next_param_index++, query.tag_filter);
    if (ec) {
      return ec;
    }
  }
  if (has_field(query.path_prefix)) {
    const std::string prefix_pattern = build_prefix_like_pattern(query.path_prefix);
    std::error_code ec = bind_text_param(stmt, next_param_index++, prefix_pattern);
    if (ec) {
      return ec;
    }
  }
  for (const auto& token : tokens) {
    const std::string token_pattern = build_contains_like_pattern(token);
    std::error_code ec = bind_text_param(stmt, next_param_index++, token_pattern);
    if (ec) {
      return ec;
    }
  }
  return {};
}

std::error_code count_attachment_hits(
    sqlite3* db,
    const SearchQuery& query,
    const std::vector<std::string>& tokens,
    std::uint64_t& out_total_hits) {
  sqlite3_stmt* stmt = nullptr;
  const std::string sql = build_attachment_count_sql(query, tokens.size());
  const int prepare_rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
  if (prepare_rc != SQLITE_OK) {
    return make_sqlite_error(prepare_rc);
  }

  int param_index = 1;
  const std::error_code bind_ec =
      bind_attachment_query_params(stmt, query, tokens, param_index);
  if (bind_ec) {
    sqlite3_finalize(stmt);
    return bind_ec;
  }

  const int step_rc = sqlite3_step(stmt);
  if (step_rc != SQLITE_ROW) {
    sqlite3_finalize(stmt);
    return make_sqlite_error(step_rc);
  }

  out_total_hits = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 0));
  sqlite3_finalize(stmt);
  return {};
}

std::string attachment_title_from_rel_path(std::string_view rel_path) {
  const std::filesystem::path path(rel_path);
  const std::filesystem::path filename = path.filename();
  if (filename.empty()) {
    return std::string(rel_path);
  }
  return filename.generic_string();
}

std::error_code append_attachment_hits(
    sqlite3* db,
    const SearchQuery& query,
    const std::vector<std::string>& tokens,
    std::vector<SearchHit>& out_hits) {
  sqlite3_stmt* stmt = nullptr;
  const std::string sql = build_attachment_select_sql(query, tokens.size());
  const int prepare_rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
  if (prepare_rc != SQLITE_OK) {
    return make_sqlite_error(prepare_rc);
  }

  int param_index = 1;
  std::error_code ec = bind_attachment_query_params(stmt, query, tokens, param_index);
  if (ec) {
    sqlite3_finalize(stmt);
    return ec;
  }
  ec = bind_int64_param(stmt, param_index++, clamp_size_for_sqlite(query.limit));
  if (ec) {
    sqlite3_finalize(stmt);
    return ec;
  }
  ec = bind_int64_param(stmt, param_index++, clamp_size_for_sqlite(query.offset));
  if (ec) {
    sqlite3_finalize(stmt);
    return ec;
  }

  while (true) {
    const int step_rc = sqlite3_step(stmt);
    if (step_rc == SQLITE_DONE) {
      break;
    }
    if (step_rc != SQLITE_ROW) {
      sqlite3_finalize(stmt);
      return make_sqlite_error(step_rc);
    }

    const unsigned char* rel_path = sqlite3_column_text(stmt, 0);
    const bool is_missing = sqlite3_column_int(stmt, 1) != 0;

    SearchHit hit;
    if (rel_path != nullptr) {
      hit.rel_path = reinterpret_cast<const char*>(rel_path);
      hit.title = attachment_title_from_rel_path(hit.rel_path);
    }
    hit.match_flags = KERNEL_SEARCH_MATCH_PATH;
    hit.snippet.clear();
    hit.snippet_status = KERNEL_SEARCH_SNIPPET_NONE;
    hit.result_kind = KERNEL_SEARCH_RESULT_ATTACHMENT;
    hit.result_flags =
        is_missing ? KERNEL_SEARCH_RESULT_FLAG_ATTACHMENT_MISSING
                   : KERNEL_SEARCH_RESULT_FLAG_NONE;
    hit.score = 0.0;
    out_hits.push_back(std::move(hit));
  }

  sqlite3_finalize(stmt);
  return {};
}

std::error_code search_note_page(
    sqlite3* db,
    const SearchQuery& query,
    const std::string& match_query,
    const std::vector<std::string>& tokens,
    SearchPage& out_page) {
  std::error_code ec = count_note_hits(db, query, match_query, out_page.total_hits);
  if (ec || query.offset >= out_page.total_hits) {
    out_page.has_more = false;
    return ec;
  }

  if (query.sort_mode == KERNEL_SEARCH_SORT_RANK_V1) {
    std::vector<SearchHit> ranked_hits;
    ec = collect_ranked_note_hits(db, query, match_query, tokens, ranked_hits);
    if (ec) {
      return ec;
    }
    move_ranked_slice(ranked_hits, query.offset, query.limit, out_page.hits);
  } else {
    const std::string rank_tag_exact_token;
    ec = append_note_hits(
        db,
        query,
        match_query,
        tokens,
        rank_tag_exact_token,
        out_page.hits);
    if (ec) {
      return ec;
    }
  }

  out_page.has_more =
      query.offset + static_cast<std::uint64_t>(out_page.hits.size()) < out_page.total_hits;
  return {};
}

std::error_code search_attachment_page(
    sqlite3* db,
    const SearchQuery& query,
    const std::vector<std::string>& tokens,
    SearchPage& out_page) {
  std::error_code ec = count_attachment_hits(db, query, tokens, out_page.total_hits);
  if (ec || query.offset >= out_page.total_hits) {
    out_page.has_more = false;
    return ec;
  }

  ec = append_attachment_hits(db, query, tokens, out_page.hits);
  if (ec) {
    return ec;
  }

  out_page.has_more =
      query.offset + static_cast<std::uint64_t>(out_page.hits.size()) < out_page.total_hits;
  return {};
}

}  // namespace

std::error_code search_page(
    kernel::storage::Database& db,
    const SearchQuery& query,
    SearchPage& out_page) {
  out_page.hits.clear();
  out_page.total_hits = 0;
  out_page.has_more = false;

  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }
  if (query.limit == 0 || query.include_deleted ||
      (query.sort_mode != KERNEL_SEARCH_SORT_REL_PATH_ASC &&
       query.sort_mode != KERNEL_SEARCH_SORT_RANK_V1) ||
      (query.kind == KERNEL_SEARCH_KIND_ATTACHMENT && has_field(query.tag_filter)) ||
      (query.kind == KERNEL_SEARCH_KIND_ATTACHMENT &&
       query.sort_mode == KERNEL_SEARCH_SORT_RANK_V1)) {
    return std::make_error_code(std::errc::invalid_argument);
  }
  if (query.kind != KERNEL_SEARCH_KIND_NOTE &&
      query.kind != KERNEL_SEARCH_KIND_ATTACHMENT &&
      query.kind != KERNEL_SEARCH_KIND_ALL) {
    return std::make_error_code(std::errc::invalid_argument);
  }

  const std::vector<std::string> tokens = split_literal_tokens(query.query);
  if (tokens.empty()) {
    return std::make_error_code(std::errc::invalid_argument);
  }
  const std::string match_query = build_literal_match_query(tokens);
  if (match_query.empty()) {
    return std::make_error_code(std::errc::invalid_argument);
  }

  if (query.kind == KERNEL_SEARCH_KIND_NOTE) {
    return search_note_page(db.connection, query, match_query, tokens, out_page);
  }
  if (query.kind == KERNEL_SEARCH_KIND_ATTACHMENT) {
    return search_attachment_page(db.connection, query, tokens, out_page);
  }

  SearchQuery note_query = query;
  note_query.kind = KERNEL_SEARCH_KIND_NOTE;
  note_query.offset = 0;
  SearchQuery attachment_query = query;
  attachment_query.kind = KERNEL_SEARCH_KIND_ATTACHMENT;
  attachment_query.offset = 0;
  attachment_query.sort_mode = KERNEL_SEARCH_SORT_REL_PATH_ASC;

  std::uint64_t note_total_hits = 0;
  std::error_code ec =
      count_note_hits(db.connection, note_query, match_query, note_total_hits);
  if (ec) {
    return ec;
  }

  std::uint64_t attachment_total_hits = 0;
  ec = count_attachment_hits(db.connection, attachment_query, tokens, attachment_total_hits);
  if (ec) {
    return ec;
  }

  out_page.total_hits = note_total_hits + attachment_total_hits;
  if (query.offset >= out_page.total_hits) {
    out_page.has_more = false;
    return {};
  }

  std::size_t remaining_limit = query.limit;
  if (query.offset < note_total_hits) {
    if (query.sort_mode == KERNEL_SEARCH_SORT_RANK_V1) {
      std::vector<SearchHit> ranked_note_hits;
      ec = collect_ranked_note_hits(
          db.connection,
          note_query,
          match_query,
          tokens,
          ranked_note_hits);
      if (ec) {
        return ec;
      }

      const std::size_t note_begin = static_cast<std::size_t>(
          std::min<std::uint64_t>(query.offset, ranked_note_hits.size()));
      const std::size_t note_end =
          std::min<std::size_t>(note_begin + remaining_limit, ranked_note_hits.size());
      out_page.hits.reserve(note_end - note_begin);
      for (std::size_t index = note_begin; index < note_end; ++index) {
        out_page.hits.push_back(std::move(ranked_note_hits[index]));
      }
      remaining_limit -= out_page.hits.size();
      attachment_query.offset = 0;
    } else {
      note_query.offset = query.offset;
      note_query.limit = std::min<std::uint64_t>(
          remaining_limit,
          note_total_hits - query.offset);
      const std::string rank_tag_exact_token;
      ec = append_note_hits(
          db.connection,
          note_query,
          match_query,
          tokens,
          rank_tag_exact_token,
          out_page.hits);
      if (ec) {
        return ec;
      }
      remaining_limit -= out_page.hits.size();
      attachment_query.offset = 0;
    }
  } else {
    attachment_query.offset = query.offset - note_total_hits;
  }

  if (remaining_limit > 0 && attachment_query.offset < attachment_total_hits) {
    attachment_query.limit = remaining_limit;
    ec = append_attachment_hits(
        db.connection,
        attachment_query,
        tokens,
        out_page.hits);
    if (ec) {
      return ec;
    }
  }

  out_page.has_more =
      query.offset + static_cast<std::uint64_t>(out_page.hits.size()) < out_page.total_hits;
  return {};
}

std::error_code search_notes(
    kernel::storage::Database& db,
    const SearchQuery& query,
    SearchPage& out_page) {
  SearchQuery note_query = query;
  note_query.kind = KERNEL_SEARCH_KIND_NOTE;
  note_query.include_deleted = false;
  note_query.sort_mode = KERNEL_SEARCH_SORT_REL_PATH_ASC;
  return search_page(db, note_query, out_page);
}

std::error_code search_notes(
    kernel::storage::Database& db,
    std::string_view query,
    std::size_t limit,
    std::vector<SearchHit>& out_hits) {
  SearchPage page;
  SearchQuery request{};
  request.query = query;
  request.limit = limit;
  request.kind = KERNEL_SEARCH_KIND_NOTE;
  const std::error_code ec = search_page(db, request, page);
  if (ec) {
    out_hits.clear();
    return ec;
  }

  out_hits = std::move(page.hits);
  return {};
}

}  // namespace kernel::search
