// Reason: This file owns note-backed search SQL so public query orchestration can stay compact.

#include "search/search_detail.h"

namespace {

std::string build_note_count_sql(const kernel::search::SearchQuery& query) {
  std::string sql =
      "SELECT COUNT(DISTINCT notes.note_id) "
      "FROM note_fts "
      "JOIN notes ON notes.note_id = note_fts.rowid ";
  if (kernel::search::detail::has_field(query.tag_filter)) {
    sql += "JOIN note_tags ON note_tags.note_id = notes.note_id ";
  }
  sql += "WHERE note_fts MATCH ? AND notes.is_deleted = 0 ";
  if (kernel::search::detail::has_field(query.tag_filter)) {
    sql += "AND note_tags.tag = ? ";
  }
  if (kernel::search::detail::has_field(query.path_prefix)) {
    sql += "AND notes.rel_path LIKE ? ESCAPE '\\' ";
  }
  sql.push_back(';');
  return sql;
}

std::string build_note_title_rank_sql(const std::size_t token_count) {
  if (token_count == 0) {
    return "0";
  }

  std::string sql = "CASE WHEN ";
  for (std::size_t index = 0; index < token_count; ++index) {
    if (index > 0) {
      sql += "AND ";
    }
    sql += "LOWER(note_fts.title) LIKE ? ESCAPE '\\' ";
  }
  sql += "THEN 1 ELSE 0 END";
  return sql;
}

std::string build_note_select_sql(
    const kernel::search::SearchQuery& query,
    const std::size_t token_count,
    const bool rank_tag_exact_enabled) {
  std::string sql =
      "SELECT DISTINCT "
      "notes.rel_path, "
      "notes.title, "
      "note_fts.title, "
      "note_fts.body, ";
  if (query.sort_mode == KERNEL_SEARCH_SORT_RANK_V1) {
    sql += "bm25(note_fts) AS raw_fts_score, ";
    if (rank_tag_exact_enabled) {
      sql +=
          "CASE WHEN EXISTS("
          "  SELECT 1 FROM note_tags AS boost_tags "
          "  WHERE boost_tags.note_id = notes.note_id AND boost_tags.tag = ?"
          ") THEN 1 ELSE 0 END AS tag_exact_hit, ";
    } else {
      sql += "0 AS tag_exact_hit, ";
    }
    sql += build_note_title_rank_sql(token_count);
    sql += " AS title_rank_hit ";
  } else {
    sql +=
        "0.0 AS raw_fts_score, "
        "0 AS tag_exact_hit, "
        "0 AS title_rank_hit ";
  }
  sql +=
      "FROM note_fts "
      "JOIN notes ON notes.note_id = note_fts.rowid ";
  if (kernel::search::detail::has_field(query.tag_filter)) {
    sql += "JOIN note_tags ON note_tags.note_id = notes.note_id ";
  }
  sql += "WHERE note_fts MATCH ? AND notes.is_deleted = 0 ";
  if (kernel::search::detail::has_field(query.tag_filter)) {
    sql += "AND note_tags.tag = ? ";
  }
  if (kernel::search::detail::has_field(query.path_prefix)) {
    sql += "AND notes.rel_path LIKE ? ESCAPE '\\' ";
  }
  if (query.sort_mode == KERNEL_SEARCH_SORT_RANK_V1) {
    sql +=
        "ORDER BY title_rank_hit DESC, "
        "tag_exact_hit DESC, "
        "raw_fts_score ASC, "
        "notes.rel_path ASC ";
  } else {
    sql += "ORDER BY notes.rel_path ASC ";
  }
  sql += "LIMIT ? OFFSET ?;";
  return sql;
}

std::error_code bind_note_query_params(
    sqlite3_stmt* stmt,
    const kernel::search::SearchQuery& query,
    const std::string& match_query,
    const std::string& rank_tag_exact_token,
    const std::vector<std::string>& tokens,
    int& next_param_index) {
  std::error_code ec;
  if (query.sort_mode == KERNEL_SEARCH_SORT_RANK_V1) {
    if (!rank_tag_exact_token.empty()) {
      ec = kernel::search::detail::bind_text_param(
          stmt,
          next_param_index++,
          rank_tag_exact_token);
      if (ec) {
        return ec;
      }
    }

    for (const auto& token : tokens) {
      ec = kernel::search::detail::bind_text_param(
          stmt,
          next_param_index++,
          kernel::search::detail::build_contains_like_pattern(token));
      if (ec) {
        return ec;
      }
    }
  }

  ec = kernel::search::detail::bind_text_param(stmt, next_param_index++, match_query);
  if (ec) {
    return ec;
  }
  if (kernel::search::detail::has_field(query.tag_filter)) {
    ec = kernel::search::detail::bind_text_param(
        stmt,
        next_param_index++,
        query.tag_filter);
    if (ec) {
      return ec;
    }
  }
  if (kernel::search::detail::has_field(query.path_prefix)) {
    ec = kernel::search::detail::bind_text_param(
        stmt,
        next_param_index++,
        kernel::search::detail::build_prefix_like_pattern(query.path_prefix));
    if (ec) {
      return ec;
    }
  }
  return {};
}

}  // namespace

namespace kernel::search::detail {

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
    ec = bind_text_param(stmt, param_index++, build_prefix_like_pattern(query.path_prefix));
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
  const std::string sql =
      build_note_select_sql(query, tokens.size(), !rank_tag_exact_token.empty());
  const int prepare_rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
  if (prepare_rc != SQLITE_OK) {
    return make_sqlite_error(prepare_rc);
  }

  int param_index = 1;
  std::error_code ec = bind_note_query_params(
      stmt,
      query,
      match_query,
      rank_tag_exact_token,
      tokens,
      param_index);
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
    hit.title_rank_hit = sqlite3_column_int(stmt, 6) != 0;
    hit.score = query.sort_mode == KERNEL_SEARCH_SORT_RANK_V1 ? -hit.raw_fts_score : 0.0;

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

  const std::string rank_tag_exact_token =
      build_rank_tag_exact_token(query, tokens);
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

  out_page.has_more =
      query.offset + static_cast<std::uint64_t>(out_page.hits.size()) < out_page.total_hits;
  return {};
}

}  // namespace kernel::search::detail
