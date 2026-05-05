// Reason: This file owns cleanup/reset helpers for search-related ABI structs.

#include "core/kernel_search_api_shared.h"

#include "core/kernel_shared.h"

namespace kernel::core::search_api {
namespace {

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

}  // namespace

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

}  // namespace kernel::core::search_api
