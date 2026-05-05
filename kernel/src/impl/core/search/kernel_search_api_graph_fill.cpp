// Reason: This file owns graph result marshalling and enriched graph JSON.

#include "core/kernel_search_api_shared.h"

#include "core/kernel_shared.h"

#include <map>
#include <new>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace kernel::core::search_api {
namespace {

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

}  // namespace

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

}  // namespace kernel::core::search_api
