// Reason: This file owns attachment-backed search SQL so attachment path retrieval can evolve without bloating the public search entrypoint.

#include "search/search_detail.h"

#include <filesystem>

namespace {

std::string build_attachment_count_sql(
    const kernel::search::SearchQuery& query,
    const std::size_t token_count) {
  std::string sql =
      "SELECT COUNT(*) FROM ("
      "SELECT DISTINCT refs.attachment_rel_path "
      "FROM note_attachment_refs AS refs "
      "JOIN notes ON notes.note_id = refs.note_id AND notes.is_deleted = 0 ";
  if (kernel::search::detail::has_field(query.tag_filter)) {
    sql += "JOIN note_tags ON note_tags.note_id = notes.note_id ";
  }
  sql += "WHERE 1=1 ";
  if (kernel::search::detail::has_field(query.tag_filter)) {
    sql += "AND note_tags.tag = ? ";
  }
  if (kernel::search::detail::has_field(query.path_prefix)) {
    sql += "AND refs.attachment_rel_path LIKE ? ESCAPE '\\' ";
  }
  for (std::size_t index = 0; index < token_count; ++index) {
    sql += "AND LOWER(refs.attachment_rel_path) LIKE ? ESCAPE '\\' ";
  }
  sql += ");";
  return sql;
}

std::string build_attachment_select_sql(
    const kernel::search::SearchQuery& query,
    const std::size_t token_count) {
  std::string sql =
      "SELECT refs.attachment_rel_path, COALESCE(MAX(attachments.is_missing), 0) "
      "FROM note_attachment_refs AS refs "
      "JOIN notes ON notes.note_id = refs.note_id AND notes.is_deleted = 0 ";
  if (kernel::search::detail::has_field(query.tag_filter)) {
    sql += "JOIN note_tags ON note_tags.note_id = notes.note_id ";
  }
  sql +=
      "LEFT JOIN attachments ON attachments.rel_path = refs.attachment_rel_path "
      "WHERE 1=1 ";
  if (kernel::search::detail::has_field(query.tag_filter)) {
    sql += "AND note_tags.tag = ? ";
  }
  if (kernel::search::detail::has_field(query.path_prefix)) {
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
    const kernel::search::SearchQuery& query,
    const std::vector<std::string>& tokens,
    int& next_param_index) {
  if (kernel::search::detail::has_field(query.tag_filter)) {
    std::error_code ec = kernel::search::detail::bind_text_param(
        stmt,
        next_param_index++,
        query.tag_filter);
    if (ec) {
      return ec;
    }
  }
  if (kernel::search::detail::has_field(query.path_prefix)) {
    std::error_code ec = kernel::search::detail::bind_text_param(
        stmt,
        next_param_index++,
        kernel::search::detail::build_prefix_like_pattern(query.path_prefix));
    if (ec) {
      return ec;
    }
  }
  for (const auto& token : tokens) {
    std::error_code ec = kernel::search::detail::bind_text_param(
        stmt,
        next_param_index++,
        kernel::search::detail::build_contains_like_pattern(token));
    if (ec) {
      return ec;
    }
  }
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

}  // namespace

namespace kernel::search::detail {

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

}  // namespace kernel::search::detail
