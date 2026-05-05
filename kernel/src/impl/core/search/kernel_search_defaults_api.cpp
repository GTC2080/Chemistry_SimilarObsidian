// Reason: This file exposes the stable default limits for search-related C APIs.

#include "core/kernel_search_api_shared.h"

#include "core/kernel_shared.h"

extern "C" kernel_status kernel_get_search_note_default_limit(std::size_t* out_limit) {
  if (out_limit == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  *out_limit = kernel::core::search_api::kDefaultSearchNoteLimit;
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_get_backlink_default_limit(std::size_t* out_limit) {
  if (out_limit == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  *out_limit = kernel::core::search_api::kDefaultBacklinkLimit;
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_get_tag_catalog_default_limit(std::size_t* out_limit) {
  if (out_limit == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  *out_limit = kernel::core::search_api::kDefaultTagCatalogLimit;
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_get_tag_note_default_limit(std::size_t* out_limit) {
  if (out_limit == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  *out_limit = kernel::core::search_api::kDefaultTagNoteLimit;
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_get_tag_tree_default_limit(std::size_t* out_limit) {
  if (out_limit == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  *out_limit = kernel::core::search_api::kDefaultTagTreeLimit;
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_get_graph_default_limit(std::size_t* out_limit) {
  if (out_limit == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  *out_limit = kernel::core::search_api::kDefaultGraphLimit;
  return kernel::core::make_status(KERNEL_OK);
}
