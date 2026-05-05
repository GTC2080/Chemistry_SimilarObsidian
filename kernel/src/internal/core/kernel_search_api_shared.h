// Reason: This file centralizes shared search ABI limits, validation, and
// result marshalling so each public query surface can stay focused.

#pragma once

#include "kernel/c_api.h"
#include "search/search.h"
#include "storage/storage.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace kernel::core::search_api {

inline constexpr std::size_t kDefaultSearchNoteLimit = 10;
inline constexpr std::size_t kDefaultBacklinkLimit = 64;
inline constexpr std::size_t kDefaultTagCatalogLimit = 512;
inline constexpr std::size_t kDefaultTagNoteLimit = 128;
inline constexpr std::size_t kDefaultTagTreeLimit = 512;
inline constexpr std::size_t kDefaultGraphLimit = 2048;

bool is_null_or_whitespace_only(const char* value);
bool optional_search_field_uses_default(const char* value);

void reset_search_results(kernel_search_results* out_results);
void reset_tag_list(kernel_tag_list* out_tags);
void reset_tag_tree(kernel_tag_tree* out_tree);
void reset_graph(kernel_graph* out_graph);
void reset_search_page(kernel_search_page* out_page);

void free_tag_list_impl(kernel_tag_list* tags);
void free_tag_tree_impl(kernel_tag_tree* tree);
void free_graph_impl(kernel_graph* graph);
void free_search_page_impl(kernel_search_page* page);

bool fill_owned_buffer(std::string_view value, kernel_owned_buffer* out_buffer);

kernel_status fill_note_list_results(
    const std::vector<kernel::storage::NoteListHit>& hits,
    kernel_search_results* out_results);
kernel_status fill_tag_list_results(
    const std::vector<kernel::storage::TagSummaryRecord>& records,
    kernel_tag_list* out_tags);
kernel_status fill_tag_tree_results(
    const std::vector<kernel::storage::TagSummaryRecord>& records,
    kernel_tag_tree* out_tree);
kernel_status fill_graph_results(
    const kernel::storage::GraphRecord& record,
    kernel_graph* out_graph);
std::string build_enriched_graph_json(const kernel::storage::GraphRecord& record);
kernel_status fill_search_results(
    const std::vector<kernel::search::SearchHit>& hits,
    kernel_search_results* out_results);
kernel_status fill_search_page_results(
    const kernel::search::SearchPage& page,
    kernel_search_page* out_page);

}  // namespace kernel::core::search_api
