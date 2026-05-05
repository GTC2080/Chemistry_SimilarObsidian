// Reason: This file keeps search-related ownership release functions isolated
// from query logic.

#include "core/kernel_search_api_shared.h"

#include "core/kernel_shared.h"

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
  kernel::core::search_api::free_tag_list_impl(tags);
}

extern "C" void kernel_free_tag_tree(kernel_tag_tree* tree) {
  kernel::core::search_api::free_tag_tree_impl(tree);
}

extern "C" void kernel_free_graph(kernel_graph* graph) {
  kernel::core::search_api::free_graph_impl(graph);
}

extern "C" void kernel_free_search_page(kernel_search_page* page) {
  kernel::core::search_api::free_search_page_impl(page);
}
