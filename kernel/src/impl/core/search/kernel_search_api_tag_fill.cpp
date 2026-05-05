// Reason: This file owns tag list and hierarchical tag tree marshalling.

#include "core/kernel_search_api_shared.h"

#include "core/kernel_shared.h"

#include <new>
#include <string>
#include <string_view>
#include <vector>

namespace kernel::core::search_api {
namespace {

struct TagTreeNodeDraft {
  std::string name;
  std::string full_path;
  std::uint32_t count = 0;
  std::vector<TagTreeNodeDraft> children;
};

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

}  // namespace

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

}  // namespace kernel::core::search_api
