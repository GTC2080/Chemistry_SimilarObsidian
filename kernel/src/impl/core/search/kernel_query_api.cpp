// Reason: This file owns search/tag/backlink query APIs and result marshalling helpers.

#include "kernel/c_api.h"

#include "core/kernel_internal.h"
#include "core/kernel_shared.h"
#include "search/search.h"
#include "storage/storage.h"

#include <cctype>
#include <cstring>
#include <map>
#include <new>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

constexpr std::size_t kDefaultSearchNoteLimit = 10;
constexpr std::size_t kDefaultBacklinkLimit = 64;
constexpr std::size_t kDefaultTagCatalogLimit = 512;
constexpr std::size_t kDefaultTagNoteLimit = 128;
constexpr std::size_t kDefaultTagTreeLimit = 512;
constexpr std::size_t kDefaultGraphLimit = 2048;

bool is_null_or_whitespace_only(const char* value) {
  if (kernel::core::is_null_or_empty(value)) {
    return true;
  }

  for (const char* cursor = value; *cursor != '\0'; ++cursor) {
    if (!std::isspace(static_cast<unsigned char>(*cursor))) {
      return false;
    }
  }
  return true;
}

bool optional_search_field_uses_default(const char* value) {
  return value == nullptr || *value == '\0';
}

void reset_search_results(kernel_search_results* out_results) {
  if (out_results == nullptr) {
    return;
  }

  kernel::core::free_search_results_impl(out_results);
  out_results->hits = nullptr;
  out_results->count = 0;
}

void free_tag_list_impl(kernel_tag_list* tags) {
  if (tags == nullptr) {
    return;
  }

  if (tags->tags != nullptr) {
    for (size_t index = 0; index < tags->count; ++index) {
      delete[] tags->tags[index].name;
      tags->tags[index].name = nullptr;
      tags->tags[index].count = 0;
    }
    delete[] tags->tags;
  }

  tags->tags = nullptr;
  tags->count = 0;
}

void reset_tag_list(kernel_tag_list* out_tags) {
  free_tag_list_impl(out_tags);
}

struct TagTreeNodeDraft {
  std::string name;
  std::string full_path;
  std::uint32_t count = 0;
  std::vector<TagTreeNodeDraft> children;
};

void free_tag_tree_nodes(kernel_tag_tree_node* nodes, const size_t count) {
  if (nodes == nullptr) {
    return;
  }

  for (size_t index = 0; index < count; ++index) {
    delete[] nodes[index].name;
    delete[] nodes[index].full_path;
    free_tag_tree_nodes(nodes[index].children, nodes[index].child_count);
    delete[] nodes[index].children;
    nodes[index].name = nullptr;
    nodes[index].full_path = nullptr;
    nodes[index].children = nullptr;
    nodes[index].child_count = 0;
    nodes[index].count = 0;
  }
}

void free_tag_tree_impl(kernel_tag_tree* tree) {
  if (tree == nullptr) {
    return;
  }

  free_tag_tree_nodes(tree->nodes, tree->count);
  delete[] tree->nodes;
  tree->nodes = nullptr;
  tree->count = 0;
}

void reset_tag_tree(kernel_tag_tree* out_tree) {
  free_tag_tree_impl(out_tree);
}

void free_graph_impl(kernel_graph* graph) {
  if (graph == nullptr) {
    return;
  }

  if (graph->nodes != nullptr) {
    for (size_t index = 0; index < graph->node_count; ++index) {
      delete[] graph->nodes[index].id;
      delete[] graph->nodes[index].name;
      graph->nodes[index].id = nullptr;
      graph->nodes[index].name = nullptr;
      graph->nodes[index].ghost = 0;
    }
    delete[] graph->nodes;
  }

  if (graph->links != nullptr) {
    for (size_t index = 0; index < graph->link_count; ++index) {
      delete[] graph->links[index].source;
      delete[] graph->links[index].target;
      delete[] graph->links[index].kind;
      graph->links[index].source = nullptr;
      graph->links[index].target = nullptr;
      graph->links[index].kind = nullptr;
    }
    delete[] graph->links;
  }

  graph->nodes = nullptr;
  graph->node_count = 0;
  graph->links = nullptr;
  graph->link_count = 0;
}

void reset_graph(kernel_graph* out_graph) {
  free_graph_impl(out_graph);
}

void free_search_page_impl(kernel_search_page* page) {
  if (page == nullptr) {
    return;
  }

  if (page->hits != nullptr) {
    for (size_t index = 0; index < page->count; ++index) {
      delete[] page->hits[index].rel_path;
      delete[] page->hits[index].title;
      delete[] page->hits[index].snippet;
    }
    delete[] page->hits;
  }

  page->hits = nullptr;
  page->count = 0;
  page->total_hits = 0;
  page->has_more = 0;
}

void reset_search_page(kernel_search_page* out_page) {
  if (out_page == nullptr) {
    return;
  }

  free_search_page_impl(out_page);
}

bool fill_owned_buffer(std::string_view value, kernel_owned_buffer* out_buffer) {
  if (out_buffer == nullptr) {
    return false;
  }

  *out_buffer = kernel_owned_buffer{};
  if (value.empty()) {
    return true;
  }

  auto* data = new (std::nothrow) char[value.size()];
  if (data == nullptr) {
    return false;
  }

  std::memcpy(data, value.data(), value.size());
  out_buffer->data = data;
  out_buffer->size = value.size();
  return true;
}

#ifdef _WIN32
std::string active_code_page_to_utf8(std::string_view value) {
  if (value.empty()) {
    return {};
  }

  const int input_size = static_cast<int>(value.size());
  const int wide_size = MultiByteToWideChar(
      CP_ACP,
      0,
      value.data(),
      input_size,
      nullptr,
      0);
  if (wide_size <= 0) {
    return std::string(value);
  }

  std::wstring wide(static_cast<std::size_t>(wide_size), L'\0');
  const int converted_wide_size = MultiByteToWideChar(
      CP_ACP,
      0,
      value.data(),
      input_size,
      wide.data(),
      wide_size);
  if (converted_wide_size <= 0) {
    return std::string(value);
  }

  const int utf8_size = WideCharToMultiByte(
      CP_UTF8,
      0,
      wide.data(),
      converted_wide_size,
      nullptr,
      0,
      nullptr,
      nullptr);
  if (utf8_size <= 0) {
    return std::string(value);
  }

  std::string result(static_cast<std::size_t>(utf8_size), '\0');
  const int converted_utf8_size = WideCharToMultiByte(
      CP_UTF8,
      0,
      wide.data(),
      converted_wide_size,
      result.data(),
      utf8_size,
      nullptr,
      nullptr);
  if (converted_utf8_size <= 0) {
    return std::string(value);
  }
  result.resize(static_cast<std::size_t>(converted_utf8_size));
  return result;
}
#else
std::string active_code_page_to_utf8(std::string_view value) {
  return std::string(value);
}
#endif

kernel_status fill_note_list_results(
    const std::vector<kernel::storage::NoteListHit>& hits,
    kernel_search_results* out_results) {
  if (hits.empty()) {
    return kernel::core::make_status(KERNEL_OK);
  }

  auto* owned_hits = new (std::nothrow) kernel_search_hit[hits.size()];
  if (owned_hits == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }

  for (size_t index = 0; index < hits.size(); ++index) {
    owned_hits[index].rel_path = nullptr;
    owned_hits[index].title = nullptr;
  }

  out_results->hits = owned_hits;
  out_results->count = hits.size();

  for (size_t index = 0; index < hits.size(); ++index) {
    out_results->hits[index].rel_path = kernel::core::duplicate_c_string(hits[index].rel_path);
    out_results->hits[index].title = kernel::core::duplicate_c_string(hits[index].title);
    out_results->hits[index].match_flags = KERNEL_SEARCH_MATCH_NONE;
    if (out_results->hits[index].rel_path == nullptr ||
        out_results->hits[index].title == nullptr) {
      kernel::core::free_search_results_impl(out_results);
      return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
    }
  }

  return kernel::core::make_status(KERNEL_OK);
}

kernel_status fill_tag_list_results(
    const std::vector<kernel::storage::TagSummaryRecord>& records,
    kernel_tag_list* out_tags) {
  if (records.empty()) {
    return kernel::core::make_status(KERNEL_OK);
  }

  auto* owned_tags = new (std::nothrow) kernel_tag_record[records.size()];
  if (owned_tags == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }

  for (size_t index = 0; index < records.size(); ++index) {
    owned_tags[index].name = nullptr;
    owned_tags[index].count = 0;
  }

  out_tags->tags = owned_tags;
  out_tags->count = records.size();

  for (size_t index = 0; index < records.size(); ++index) {
    out_tags->tags[index].name = kernel::core::duplicate_c_string(records[index].name);
    out_tags->tags[index].count = records[index].count;
    if (out_tags->tags[index].name == nullptr) {
      free_tag_list_impl(out_tags);
      return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
    }
  }

  return kernel::core::make_status(KERNEL_OK);
}

std::vector<std::string_view> tag_path_parts(std::string_view tag_name) {
  std::vector<std::string_view> parts;
  std::size_t cursor = 0;
  while (cursor <= tag_name.size()) {
    const std::size_t slash = tag_name.find('/', cursor);
    const std::size_t end = slash == std::string_view::npos ? tag_name.size() : slash;
    if (end > cursor) {
      parts.push_back(tag_name.substr(cursor, end - cursor));
    }
    if (slash == std::string_view::npos) {
      break;
    }
    cursor = slash + 1;
  }
  return parts;
}

TagTreeNodeDraft* find_tag_tree_node(
    std::vector<TagTreeNodeDraft>& nodes,
    std::string_view name) {
  for (auto& node : nodes) {
    if (node.name == name) {
      return &node;
    }
  }
  return nullptr;
}

void append_tag_tree_record(
    std::vector<TagTreeNodeDraft>& roots,
    const kernel::storage::TagSummaryRecord& record) {
  const std::vector<std::string_view> parts = tag_path_parts(record.name);
  if (parts.empty()) {
    return;
  }

  std::vector<TagTreeNodeDraft>* level = &roots;
  std::string full_path;
  for (const std::string_view part : parts) {
    if (!full_path.empty()) {
      full_path += "/";
    }
    full_path.append(part.data(), part.size());

    TagTreeNodeDraft* node = find_tag_tree_node(*level, part);
    if (node == nullptr) {
      level->push_back(TagTreeNodeDraft{
          std::string(part),
          full_path,
          full_path == record.name ? record.count : 0,
          {}});
      node = &level->back();
    } else if (full_path == record.name) {
      node->count = record.count;
    }
    level = &node->children;
  }
}

kernel_status fill_tag_tree_node(
    const TagTreeNodeDraft& draft,
    kernel_tag_tree_node* out_node) {
  out_node->name = nullptr;
  out_node->full_path = nullptr;
  out_node->count = draft.count;
  out_node->children = nullptr;
  out_node->child_count = 0;

  out_node->name = kernel::core::duplicate_c_string(draft.name);
  out_node->full_path = kernel::core::duplicate_c_string(draft.full_path);
  if (out_node->name == nullptr || out_node->full_path == nullptr) {
    delete[] out_node->name;
    delete[] out_node->full_path;
    out_node->name = nullptr;
    out_node->full_path = nullptr;
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }

  if (draft.children.empty()) {
    return kernel::core::make_status(KERNEL_OK);
  }

  auto* children = new (std::nothrow) kernel_tag_tree_node[draft.children.size()];
  if (children == nullptr) {
    delete[] out_node->name;
    delete[] out_node->full_path;
    out_node->name = nullptr;
    out_node->full_path = nullptr;
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }

  out_node->children = children;
  out_node->child_count = draft.children.size();
  for (size_t index = 0; index < draft.children.size(); ++index) {
    children[index].name = nullptr;
    children[index].full_path = nullptr;
    children[index].count = 0;
    children[index].children = nullptr;
    children[index].child_count = 0;
  }
  for (size_t index = 0; index < draft.children.size(); ++index) {
    const kernel_status status = fill_tag_tree_node(draft.children[index], &children[index]);
    if (status.code != KERNEL_OK) {
      free_tag_tree_nodes(out_node->children, out_node->child_count);
      delete[] out_node->children;
      delete[] out_node->name;
      delete[] out_node->full_path;
      out_node->children = nullptr;
      out_node->child_count = 0;
      out_node->name = nullptr;
      out_node->full_path = nullptr;
      out_node->count = 0;
      return status;
    }
  }

  return kernel::core::make_status(KERNEL_OK);
}

kernel_status fill_tag_tree_results(
    const std::vector<kernel::storage::TagSummaryRecord>& records,
    kernel_tag_tree* out_tree) {
  std::vector<TagTreeNodeDraft> roots;
  for (const auto& record : records) {
    append_tag_tree_record(roots, record);
  }

  if (roots.empty()) {
    return kernel::core::make_status(KERNEL_OK);
  }

  auto* nodes = new (std::nothrow) kernel_tag_tree_node[roots.size()];
  if (nodes == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }

  out_tree->nodes = nodes;
  out_tree->count = roots.size();
  for (size_t index = 0; index < roots.size(); ++index) {
    nodes[index].name = nullptr;
    nodes[index].full_path = nullptr;
    nodes[index].count = 0;
    nodes[index].children = nullptr;
    nodes[index].child_count = 0;
  }
  for (size_t index = 0; index < roots.size(); ++index) {
    const kernel_status status = fill_tag_tree_node(roots[index], &nodes[index]);
    if (status.code != KERNEL_OK) {
      free_tag_tree_impl(out_tree);
      return status;
    }
  }

  return kernel::core::make_status(KERNEL_OK);
}

kernel_status fill_graph_results(
    const kernel::storage::GraphRecord& record,
    kernel_graph* out_graph) {
  if (!record.nodes.empty()) {
    auto* owned_nodes = new (std::nothrow) kernel_graph_node[record.nodes.size()];
    if (owned_nodes == nullptr) {
      return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
    }

    for (size_t index = 0; index < record.nodes.size(); ++index) {
      owned_nodes[index].id = nullptr;
      owned_nodes[index].name = nullptr;
      owned_nodes[index].ghost = 0;
    }

    out_graph->nodes = owned_nodes;
    out_graph->node_count = record.nodes.size();

    for (size_t index = 0; index < record.nodes.size(); ++index) {
      out_graph->nodes[index].id =
          kernel::core::duplicate_c_string(record.nodes[index].id);
      out_graph->nodes[index].name =
          kernel::core::duplicate_c_string(record.nodes[index].name);
      out_graph->nodes[index].ghost = record.nodes[index].ghost ? 1 : 0;
      if (out_graph->nodes[index].id == nullptr ||
          out_graph->nodes[index].name == nullptr) {
        free_graph_impl(out_graph);
        return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
      }
    }
  }

  if (!record.links.empty()) {
    auto* owned_links = new (std::nothrow) kernel_graph_link[record.links.size()];
    if (owned_links == nullptr) {
      free_graph_impl(out_graph);
      return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
    }

    for (size_t index = 0; index < record.links.size(); ++index) {
      owned_links[index].source = nullptr;
      owned_links[index].target = nullptr;
      owned_links[index].kind = nullptr;
    }

    out_graph->links = owned_links;
    out_graph->link_count = record.links.size();

    for (size_t index = 0; index < record.links.size(); ++index) {
      out_graph->links[index].source =
          kernel::core::duplicate_c_string(record.links[index].source);
      out_graph->links[index].target =
          kernel::core::duplicate_c_string(record.links[index].target);
      out_graph->links[index].kind =
          kernel::core::duplicate_c_string(record.links[index].kind);
      if (out_graph->links[index].source == nullptr ||
          out_graph->links[index].target == nullptr ||
          out_graph->links[index].kind == nullptr) {
        free_graph_impl(out_graph);
        return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
      }
    }
  }

  return kernel::core::make_status(KERNEL_OK);
}

std::string build_enriched_graph_json(const kernel::storage::GraphRecord& record) {
  std::map<std::string, std::vector<std::string>> neighbors;
  std::vector<std::pair<std::string, std::string>> link_pairs;
  link_pairs.reserve(record.links.size() * 2);

  for (const auto& link : record.links) {
    neighbors[link.source].push_back(link.target);
    neighbors[link.target].push_back(link.source);
    link_pairs.emplace_back(link.source, link.target);
    link_pairs.emplace_back(link.target, link.source);
  }

  std::string json = "{\"nodes\":[";
  for (size_t index = 0; index < record.nodes.size(); ++index) {
    if (index != 0) {
      json += ",";
    }
    json += "{\"id\":\"" +
            kernel::core::json_escape(active_code_page_to_utf8(record.nodes[index].id)) + "\",";
    json += "\"name\":\"" + kernel::core::json_escape(record.nodes[index].name) + "\",";
    json += "\"ghost\":";
    json += record.nodes[index].ghost ? "true" : "false";
    json += "}";
  }

  json += "],\"links\":[";
  for (size_t index = 0; index < record.links.size(); ++index) {
    if (index != 0) {
      json += ",";
    }
    json += "{\"source\":\"" +
            kernel::core::json_escape(active_code_page_to_utf8(record.links[index].source)) +
            "\",";
    json += "\"target\":\"" +
            kernel::core::json_escape(active_code_page_to_utf8(record.links[index].target)) +
            "\",";
    json += "\"kind\":\"" + kernel::core::json_escape(record.links[index].kind) + "\"}";
  }

  json += "],\"neighbors\":{";
  bool first_neighbor = true;
  for (const auto& [node_id, entries] : neighbors) {
    if (!first_neighbor) {
      json += ",";
    }
    first_neighbor = false;
    json += "\"" + kernel::core::json_escape(active_code_page_to_utf8(node_id)) + "\":[";
    for (size_t index = 0; index < entries.size(); ++index) {
      if (index != 0) {
        json += ",";
      }
      json += "\"" + kernel::core::json_escape(active_code_page_to_utf8(entries[index])) + "\"";
    }
    json += "]";
  }

  json += "},\"linkPairs\":[";
  for (size_t index = 0; index < link_pairs.size(); ++index) {
    if (index != 0) {
      json += ",";
    }
    const std::string source = active_code_page_to_utf8(link_pairs[index].first);
    const std::string target = active_code_page_to_utf8(link_pairs[index].second);
    json += "\"" + kernel::core::json_escape(source + "->" + target) + "\"";
  }
  json += "]}";
  return json;
}

kernel_status fill_search_results(
    const std::vector<kernel::search::SearchHit>& hits,
    kernel_search_results* out_results) {
  if (hits.empty()) {
    return kernel::core::make_status(KERNEL_OK);
  }

  auto* owned_hits = new (std::nothrow) kernel_search_hit[hits.size()];
  if (owned_hits == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }

  for (size_t index = 0; index < hits.size(); ++index) {
    owned_hits[index].rel_path = nullptr;
    owned_hits[index].title = nullptr;
    owned_hits[index].match_flags = KERNEL_SEARCH_MATCH_NONE;
  }

  out_results->hits = owned_hits;
  out_results->count = hits.size();

  for (size_t index = 0; index < hits.size(); ++index) {
    out_results->hits[index].rel_path = kernel::core::duplicate_c_string(hits[index].rel_path);
    out_results->hits[index].title = kernel::core::duplicate_c_string(hits[index].title);
    out_results->hits[index].match_flags = hits[index].match_flags;
    if (out_results->hits[index].rel_path == nullptr ||
        out_results->hits[index].title == nullptr) {
      kernel::core::free_search_results_impl(out_results);
      return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
    }
  }

  return kernel::core::make_status(KERNEL_OK);
}

kernel_status fill_search_page_results(
    const kernel::search::SearchPage& page,
    kernel_search_page* out_page) {
  out_page->total_hits = page.total_hits;
  out_page->has_more = page.has_more ? 1 : 0;
  if (page.hits.empty()) {
    return kernel::core::make_status(KERNEL_OK);
  }

  auto* owned_hits = new (std::nothrow) kernel_search_page_hit[page.hits.size()];
  if (owned_hits == nullptr) {
    out_page->total_hits = 0;
    out_page->has_more = 0;
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }

  for (size_t index = 0; index < page.hits.size(); ++index) {
    owned_hits[index].rel_path = nullptr;
    owned_hits[index].title = nullptr;
    owned_hits[index].snippet = nullptr;
    owned_hits[index].match_flags = KERNEL_SEARCH_MATCH_NONE;
    owned_hits[index].snippet_status = KERNEL_SEARCH_SNIPPET_NONE;
    owned_hits[index].result_kind = KERNEL_SEARCH_RESULT_NOTE;
    owned_hits[index].result_flags = KERNEL_SEARCH_RESULT_FLAG_NONE;
    owned_hits[index].score = 0.0;
  }

  out_page->hits = owned_hits;
  out_page->count = page.hits.size();

  for (size_t index = 0; index < page.hits.size(); ++index) {
    out_page->hits[index].rel_path =
        kernel::core::duplicate_c_string(page.hits[index].rel_path);
    out_page->hits[index].title =
        kernel::core::duplicate_c_string(page.hits[index].title);
    out_page->hits[index].snippet =
        kernel::core::duplicate_c_string(page.hits[index].snippet);
    out_page->hits[index].match_flags = page.hits[index].match_flags;
    out_page->hits[index].snippet_status = page.hits[index].snippet_status;
    out_page->hits[index].result_kind = page.hits[index].result_kind;
    out_page->hits[index].result_flags = page.hits[index].result_flags;
    out_page->hits[index].score = page.hits[index].score;
    if (out_page->hits[index].rel_path == nullptr ||
        out_page->hits[index].title == nullptr ||
        out_page->hits[index].snippet == nullptr) {
      free_search_page_impl(out_page);
      return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
    }
  }

  return kernel::core::make_status(KERNEL_OK);
}

}  // namespace

extern "C" kernel_status kernel_get_search_note_default_limit(std::size_t* out_limit) {
  if (out_limit == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  *out_limit = kDefaultSearchNoteLimit;
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_get_backlink_default_limit(std::size_t* out_limit) {
  if (out_limit == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  *out_limit = kDefaultBacklinkLimit;
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_get_tag_catalog_default_limit(std::size_t* out_limit) {
  if (out_limit == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  *out_limit = kDefaultTagCatalogLimit;
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_get_tag_note_default_limit(std::size_t* out_limit) {
  if (out_limit == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  *out_limit = kDefaultTagNoteLimit;
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_get_tag_tree_default_limit(std::size_t* out_limit) {
  if (out_limit == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  *out_limit = kDefaultTagTreeLimit;
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_get_graph_default_limit(std::size_t* out_limit) {
  if (out_limit == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  *out_limit = kDefaultGraphLimit;
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_search_notes(
    kernel_handle* handle,
    const char* query,
    kernel_search_results* out_results) {
  return kernel_search_notes_limited(handle, query, static_cast<size_t>(-1), out_results);
}

extern "C" kernel_status kernel_search_notes_limited(
    kernel_handle* handle,
    const char* query,
    size_t limit,
    kernel_search_results* out_results) {
  reset_search_results(out_results);
  if (handle == nullptr || kernel::core::is_null_or_empty(query) || out_results == nullptr ||
      limit == 0) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::vector<kernel::search::SearchHit> hits;
  {
    std::lock_guard lock(handle->storage_mutex);
    const std::error_code search_ec = kernel::search::search_notes(handle->storage, query, limit, hits);
    if (search_ec) {
      return kernel::core::make_status(kernel::core::map_error(search_ec));
    }
  }

  return fill_search_results(hits, out_results);
}

extern "C" kernel_status kernel_query_search(
    kernel_handle* handle,
    const kernel_search_query* request,
    kernel_search_page* out_page) {
  reset_search_page(out_page);
  if (handle == nullptr || request == nullptr || out_page == nullptr ||
      is_null_or_whitespace_only(request->query) || request->limit == 0 ||
      request->limit > kernel::search::kSearchPageMaxLimit ||
      request->include_deleted != 0 ||
      (request->sort_mode != KERNEL_SEARCH_SORT_REL_PATH_ASC &&
       request->sort_mode != KERNEL_SEARCH_SORT_RANK_V1)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  if (request->kind != KERNEL_SEARCH_KIND_NOTE &&
      request->kind != KERNEL_SEARCH_KIND_ATTACHMENT &&
      request->kind != KERNEL_SEARCH_KIND_ALL) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  if (!optional_search_field_uses_default(request->tag_filter) &&
      is_null_or_whitespace_only(request->tag_filter)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  if (request->kind == KERNEL_SEARCH_KIND_ATTACHMENT &&
      !optional_search_field_uses_default(request->tag_filter)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  if (request->kind == KERNEL_SEARCH_KIND_ATTACHMENT &&
      request->sort_mode == KERNEL_SEARCH_SORT_RANK_V1) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::string normalized_path_prefix;
  if (!optional_search_field_uses_default(request->path_prefix)) {
    if (is_null_or_whitespace_only(request->path_prefix) ||
        !kernel::core::is_valid_relative_path(request->path_prefix)) {
      return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
    }
    normalized_path_prefix = kernel::core::normalize_rel_path(request->path_prefix);
  }

  kernel::search::SearchPage page;
  {
    std::lock_guard lock(handle->storage_mutex);
    const std::error_code search_ec =
        kernel::search::search_page(
            handle->storage,
            kernel::search::SearchQuery{
                request->query,
                request->limit,
                request->offset,
                request->kind,
                optional_search_field_uses_default(request->tag_filter)
                    ? std::string_view{}
                    : std::string_view(request->tag_filter),
                normalized_path_prefix,
                false,
                request->sort_mode},
            page);
    if (search_ec) {
      return kernel::core::make_status(kernel::core::map_error(search_ec));
    }
  }

  return fill_search_page_results(page, out_page);
}

extern "C" kernel_status kernel_query_tag_notes(
    kernel_handle* handle,
    const char* tag,
    size_t limit,
    kernel_search_results* out_results) {
  reset_search_results(out_results);
  if (handle == nullptr || is_null_or_whitespace_only(tag) || limit == 0 || out_results == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::vector<kernel::storage::NoteListHit> hits;
  {
    std::lock_guard lock(handle->storage_mutex);
    const std::error_code ec = kernel::storage::list_notes_by_tag(handle->storage, tag, limit, hits);
    if (ec) {
      return kernel::core::make_status(kernel::core::map_error(ec));
    }
  }

  return fill_note_list_results(hits, out_results);
}

extern "C" kernel_status kernel_query_tags(
    kernel_handle* handle,
    size_t limit,
    kernel_tag_list* out_tags) {
  reset_tag_list(out_tags);
  if (handle == nullptr || limit == 0 || out_tags == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::vector<kernel::storage::TagSummaryRecord> records;
  {
    std::lock_guard lock(handle->storage_mutex);
    const std::error_code ec =
        kernel::storage::list_tag_summaries(handle->storage, limit, records);
    if (ec) {
      return kernel::core::make_status(kernel::core::map_error(ec));
    }
  }

  return fill_tag_list_results(records, out_tags);
}

extern "C" kernel_status kernel_query_tag_tree(
    kernel_handle* handle,
    size_t limit,
    kernel_tag_tree* out_tree) {
  reset_tag_tree(out_tree);
  if (handle == nullptr || limit == 0 || out_tree == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::vector<kernel::storage::TagSummaryRecord> records;
  {
    std::lock_guard lock(handle->storage_mutex);
    const std::error_code ec =
        kernel::storage::list_tag_summaries(handle->storage, limit, records);
    if (ec) {
      return kernel::core::make_status(kernel::core::map_error(ec));
    }
  }

  return fill_tag_tree_results(records, out_tree);
}

extern "C" kernel_status kernel_query_graph(
    kernel_handle* handle,
    size_t note_limit,
    kernel_graph* out_graph) {
  reset_graph(out_graph);
  if (handle == nullptr || note_limit == 0 || out_graph == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel::storage::GraphRecord graph;
  {
    std::lock_guard lock(handle->storage_mutex);
    const std::error_code ec =
        kernel::storage::build_note_graph(handle->storage, note_limit, graph);
    if (ec) {
      return kernel::core::make_status(kernel::core::map_error(ec));
    }
  }

  return fill_graph_results(graph, out_graph);
}

extern "C" kernel_status kernel_query_enriched_graph_json(
    kernel_handle* handle,
    size_t note_limit,
    kernel_owned_buffer* out_buffer) {
  if (out_buffer == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  *out_buffer = kernel_owned_buffer{};
  if (handle == nullptr || note_limit == 0) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel::storage::GraphRecord graph;
  {
    std::lock_guard lock(handle->storage_mutex);
    const std::error_code ec =
        kernel::storage::build_note_graph(handle->storage, note_limit, graph);
    if (ec) {
      return kernel::core::make_status(kernel::core::map_error(ec));
    }
  }

  const std::string json = build_enriched_graph_json(graph);
  if (!fill_owned_buffer(json, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_query_backlinks(
    kernel_handle* handle,
    const char* rel_path,
    size_t limit,
    kernel_search_results* out_results) {
  reset_search_results(out_results);
  if (handle == nullptr || limit == 0 || out_results == nullptr ||
      !kernel::core::is_valid_relative_path(rel_path)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string normalized_rel_path = kernel::core::normalize_rel_path(rel_path);

  std::vector<kernel::storage::NoteListHit> hits;
  {
    std::lock_guard lock(handle->storage_mutex);
    const std::error_code ec =
        kernel::storage::list_backlinks_for_rel_path(
            handle->storage,
            normalized_rel_path,
            limit,
            hits);
    if (ec) {
      return kernel::core::make_status(kernel::core::map_error(ec));
    }
  }

  return fill_note_list_results(hits, out_results);
}

extern "C" void kernel_free_buffer(kernel_owned_buffer* buffer) {
  if (buffer == nullptr) {
    return;
  }

  delete[] buffer->data;
  buffer->data = nullptr;
  buffer->size = 0;
}

extern "C" void kernel_free_search_results(kernel_search_results* results) {
  kernel::core::free_search_results_impl(results);
}

extern "C" void kernel_free_tag_list(kernel_tag_list* tags) {
  free_tag_list_impl(tags);
}

extern "C" void kernel_free_tag_tree(kernel_tag_tree* tree) {
  free_tag_tree_impl(tree);
}

extern "C" void kernel_free_graph(kernel_graph* graph) {
  free_graph_impl(graph);
}

extern "C" void kernel_free_search_page(kernel_search_page* page) {
  free_search_page_impl(page);
}
