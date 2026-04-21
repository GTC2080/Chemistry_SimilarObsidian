#pragma once

#include "kernel/c_api.h"

#include <cstddef>
#include <string>

inline std::string two_digit_index(const int value) {
  if (value < 10) {
    return "0" + std::to_string(value);
  }
  return std::to_string(value);
}

inline kernel_search_query make_default_search_query(const char* query, const std::size_t limit) {
  kernel_search_query request{};
  request.query = query;
  request.limit = limit;
  request.offset = 0;
  request.kind = KERNEL_SEARCH_KIND_NOTE;
  request.tag_filter = nullptr;
  request.path_prefix = nullptr;
  request.include_deleted = 0;
  request.sort_mode = KERNEL_SEARCH_SORT_REL_PATH_ASC;
  return request;
}
