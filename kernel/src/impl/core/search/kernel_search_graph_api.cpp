// Reason: This file owns note graph query C APIs.

#include "core/kernel_search_api_shared.h"

#include "core/kernel_internal.h"
#include "core/kernel_shared.h"

#include <mutex>
#include <string>

extern "C" kernel_status kernel_query_graph(
    kernel_handle* handle,
    size_t note_limit,
    kernel_graph* out_graph) {
  kernel::core::search_api::reset_graph(out_graph);
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

  return kernel::core::search_api::fill_graph_results(graph, out_graph);
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

  const std::string json = kernel::core::search_api::build_enriched_graph_json(graph);
  if (!kernel::core::search_api::fill_owned_buffer(json, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}
