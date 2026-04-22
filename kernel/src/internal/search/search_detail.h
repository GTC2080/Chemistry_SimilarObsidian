#pragma once

#include "search/search.h"
#include "third_party/sqlite/sqlite3.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace kernel::search::detail {

bool has_field(std::string_view value);

std::error_code make_sqlite_error(int rc);

std::error_code bind_text_param(
    sqlite3_stmt* stmt,
    int index,
    std::string_view text);

std::error_code bind_int64_param(
    sqlite3_stmt* stmt,
    int index,
    sqlite3_int64 value);

std::vector<std::string> split_literal_tokens(std::string_view query);

std::string build_literal_match_query(const std::vector<std::string>& tokens);

std::string build_prefix_like_pattern(std::string_view prefix);

std::string build_contains_like_pattern(std::string_view token);

bool contains_all_tokens_case_insensitive(
    std::string_view haystack,
    const std::vector<std::string>& tokens);

std::string_view strip_leading_title_heading(
    std::string_view body_text,
    std::string_view title);

sqlite3_int64 clamp_size_for_sqlite(std::size_t value);

std::string build_body_snippet(
    std::string_view body_text,
    const std::vector<std::string>& tokens);

std::string build_rank_tag_exact_token(
    const SearchQuery& query,
    const std::vector<std::string>& tokens);

std::error_code count_note_hits(
    sqlite3* db,
    const SearchQuery& query,
    const std::string& match_query,
    std::uint64_t& out_total_hits);

std::error_code append_note_hits(
    sqlite3* db,
    const SearchQuery& query,
    const std::string& match_query,
    const std::vector<std::string>& tokens,
    const std::string& rank_tag_exact_token,
    std::vector<SearchHit>& out_hits);

std::error_code search_note_page(
    sqlite3* db,
    const SearchQuery& query,
    const std::string& match_query,
    const std::vector<std::string>& tokens,
    SearchPage& out_page);

std::error_code count_attachment_hits(
    sqlite3* db,
    const SearchQuery& query,
    const std::vector<std::string>& tokens,
    std::uint64_t& out_total_hits);

std::error_code append_attachment_hits(
    sqlite3* db,
    const SearchQuery& query,
    const std::vector<std::string>& tokens,
    std::vector<SearchHit>& out_hits);

std::error_code search_attachment_page(
    sqlite3* db,
    const SearchQuery& query,
    const std::vector<std::string>& tokens,
    SearchPage& out_page);

}  // namespace kernel::search::detail
