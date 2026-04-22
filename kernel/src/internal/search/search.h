// Reason: This file defines the smallest internal search API over the local SQLite state store.

#pragma once

#include "kernel/types.h"
#include "storage/storage.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace kernel::search {

inline constexpr std::string_view kSearchContractRevision = "track1_batch4_ranking_v1";
inline constexpr std::string_view kSearchBackend = "sqlite_fts5";
inline constexpr std::string_view kSearchSnippetMode =
    "body_single_segment_plaintext_fixed_length";
inline constexpr std::string_view kSearchPaginationMode = "offset_limit_exact_total_v1";
inline constexpr std::string_view kSearchFiltersMode = "kind_tag_path_prefix_v1";
inline constexpr std::string_view kSearchRankingMode = "fts_title_tag_v1";
inline constexpr std::string_view kSearchSupportedKinds = "note,attachment,all";
inline constexpr std::string_view kSearchSupportedFilters = "kind,tag,path_prefix";
inline constexpr std::string_view kSearchAllKindOrder =
    "notes_then_attachments_rel_path_asc";
inline constexpr std::string_view kSearchRankingSupportedKinds =
    "note,all_note_branch";
inline constexpr std::string_view kSearchRankingTieBreak = "rel_path_asc";
inline constexpr std::size_t kSearchSnippetMaxBytes = 160;
inline constexpr std::size_t kSearchPageMaxLimit = 128;
inline constexpr bool kSearchTotalHitsSupported = true;
inline constexpr bool kSearchIncludeDeletedSupported = false;
inline constexpr bool kSearchAttachmentPathOnly = true;
inline constexpr bool kSearchTitleHitBoostEnabled = true;
inline constexpr bool kSearchTagExactBoostEnabled = true;
inline constexpr bool kSearchTagExactBoostSingleTokenOnly = true;

struct SearchQuery {
  std::string_view query;
  std::size_t limit = static_cast<std::size_t>(-1);
  std::size_t offset = 0;
  kernel_search_kind kind = KERNEL_SEARCH_KIND_NOTE;
  std::string_view tag_filter;
  std::string_view path_prefix;
  bool include_deleted = false;
  kernel_search_sort_mode sort_mode = KERNEL_SEARCH_SORT_REL_PATH_ASC;
};

struct SearchHit {
  std::string rel_path;
  std::string title;
  std::string snippet;
  std::uint32_t match_flags = 0;
  kernel_search_snippet_status snippet_status = KERNEL_SEARCH_SNIPPET_NONE;
  kernel_search_result_kind result_kind = KERNEL_SEARCH_RESULT_NOTE;
  std::uint32_t result_flags = KERNEL_SEARCH_RESULT_FLAG_NONE;
  double score = 0.0;
  double raw_fts_score = 0.0;
  bool title_rank_hit = false;
  bool tag_exact_rank_hit = false;
};

struct SearchPage {
  std::vector<SearchHit> hits;
  std::uint64_t total_hits = 0;
  bool has_more = false;
};

std::error_code search_notes(
    kernel::storage::Database& db,
    const SearchQuery& query,
    SearchPage& out_page);

std::error_code search_page(
    kernel::storage::Database& db,
    const SearchQuery& query,
    SearchPage& out_page);

std::error_code search_notes(
    kernel::storage::Database& db,
    std::string_view query,
    std::size_t limit,
    std::vector<SearchHit>& out_hits);

inline std::error_code search_notes(
    kernel::storage::Database& db,
    std::string_view query,
    std::vector<SearchHit>& out_hits) {
  return search_notes(db, query, static_cast<std::size_t>(-1), out_hits);
}

}  // namespace kernel::search
