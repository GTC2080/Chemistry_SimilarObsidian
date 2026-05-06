// Reason: This file keeps the host-facing search contract orchestration compact while detail-heavy SQL lives in focused units.

#include "search/search.h"
#include "search/search_detail.h"

#include <algorithm>

namespace kernel::search {
namespace {

bool is_invalid_search_query(const SearchQuery& query) {
  return query.limit == 0 || query.include_deleted ||
         (query.sort_mode != KERNEL_SEARCH_SORT_REL_PATH_ASC &&
          query.sort_mode != KERNEL_SEARCH_SORT_RANK_V1) ||
         (query.kind == KERNEL_SEARCH_KIND_ATTACHMENT &&
          detail::has_field(query.tag_filter)) ||
         (query.kind == KERNEL_SEARCH_KIND_ATTACHMENT &&
          query.sort_mode == KERNEL_SEARCH_SORT_RANK_V1) ||
         (query.kind != KERNEL_SEARCH_KIND_NOTE &&
          query.kind != KERNEL_SEARCH_KIND_ATTACHMENT &&
          query.kind != KERNEL_SEARCH_KIND_ALL);
}

}  // namespace

std::error_code search_page(
    kernel::storage::Database& db,
    const SearchQuery& query,
    SearchPage& out_page) {
  out_page = SearchPage{};

  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }
  if (is_invalid_search_query(query)) {
    return std::make_error_code(std::errc::invalid_argument);
  }

  const std::vector<std::string> tokens = detail::split_literal_tokens(query.query);
  if (tokens.empty()) {
    return std::make_error_code(std::errc::invalid_argument);
  }
  const std::string match_query = detail::build_literal_match_query(tokens);
  if (match_query.empty()) {
    return std::make_error_code(std::errc::invalid_argument);
  }

  if (query.kind == KERNEL_SEARCH_KIND_NOTE) {
    return detail::search_note_page(db.connection, query, match_query, tokens, out_page);
  }
  if (query.kind == KERNEL_SEARCH_KIND_ATTACHMENT) {
    return detail::search_attachment_page(db.connection, query, tokens, out_page);
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
      detail::count_note_hits(db.connection, note_query, match_query, note_total_hits);
  if (ec) {
    return ec;
  }

  std::uint64_t attachment_total_hits = 0;
  ec = detail::count_attachment_hits(
      db.connection,
      attachment_query,
      tokens,
      attachment_total_hits);
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
    note_query.offset = query.offset;
    note_query.limit = std::min<std::uint64_t>(
        remaining_limit,
        note_total_hits - query.offset);
    const std::string rank_tag_exact_token =
        detail::build_rank_tag_exact_token(note_query, tokens);
    ec = detail::append_note_hits(
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
  } else {
    attachment_query.offset = query.offset - note_total_hits;
  }

  if (remaining_limit > 0 && attachment_query.offset < attachment_total_hits) {
    attachment_query.limit = remaining_limit;
    ec = detail::append_attachment_hits(
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

std::error_code search_notes_compact(
    kernel::storage::Database& db,
    std::string_view query,
    std::size_t limit,
    std::vector<SearchHit>& out_hits) {
  out_hits.clear();
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }
  if (limit == 0) {
    return std::make_error_code(std::errc::invalid_argument);
  }

  const std::vector<std::string> tokens = detail::split_literal_tokens(query);
  if (tokens.empty()) {
    return std::make_error_code(std::errc::invalid_argument);
  }
  const std::string match_query = detail::build_literal_match_query(tokens);
  if (match_query.empty()) {
    return std::make_error_code(std::errc::invalid_argument);
  }

  const std::error_code ec = detail::append_note_hits_compact(
      db.connection,
      match_query,
      tokens,
      limit,
      out_hits);
  if (ec) {
    out_hits.clear();
    return ec;
  }
  return {};
}

}  // namespace kernel::search
