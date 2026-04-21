// Reason: This file owns search/tag/backlink query APIs and result marshalling helpers.

#include "kernel/c_api.h"

#include "core/kernel_internal.h"
#include "core/kernel_shared.h"
#include "search/search.h"
#include "storage/storage.h"

#include <cctype>
#include <new>
#include <string>
#include <vector>

namespace {

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

extern "C" void kernel_free_search_page(kernel_search_page* page) {
  free_search_page_impl(page);
}
