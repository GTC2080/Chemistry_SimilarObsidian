// Reason: This file owns the host-facing note catalog query surface.

#include "kernel/c_api.h"

#include "core/kernel_internal.h"
#include "core/kernel_shared.h"
#include "storage/storage.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <new>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::size_t kDefaultNoteCatalogLimit = 100000;
constexpr std::size_t kDefaultVaultScanLimit = 4096;

void free_note_record_impl(kernel_note_record* note) {
  if (note == nullptr) {
    return;
  }

  delete[] note->rel_path;
  delete[] note->title;
  note->rel_path = nullptr;
  note->title = nullptr;
  note->file_size = 0;
  note->mtime_ns = 0;
  note->content_revision[0] = '\0';
}

void free_note_list_impl(kernel_note_list* notes) {
  if (notes == nullptr) {
    return;
  }

  if (notes->notes != nullptr) {
    for (size_t index = 0; index < notes->count; ++index) {
      free_note_record_impl(&notes->notes[index]);
    }
    delete[] notes->notes;
  }

  notes->notes = nullptr;
  notes->count = 0;
}

void reset_note_list(kernel_note_list* notes) {
  free_note_list_impl(notes);
}

std::string trim_ignored_root(std::string_view value) {
  std::size_t start = 0;
  while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
    ++start;
  }
  while (start < value.size() && (value[start] == '/' || value[start] == '\\')) {
    ++start;
  }

  std::size_t end = value.size();
  while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
    --end;
  }
  while (end > start && (value[end - 1] == '/' || value[end - 1] == '\\')) {
    --end;
  }

  return std::string(value.substr(start, end - start));
}

std::set<std::string> parse_ignored_roots(const char* ignored_roots_csv) {
  std::set<std::string> ignored;
  if (ignored_roots_csv == nullptr || ignored_roots_csv[0] == '\0') {
    return ignored;
  }

  const std::string_view raw(ignored_roots_csv);
  std::size_t start = 0;
  while (start <= raw.size()) {
    const std::size_t next = raw.find(',', start);
    const std::string root = trim_ignored_root(
        next == std::string_view::npos ? raw.substr(start) : raw.substr(start, next - start));
    if (!root.empty()) {
      ignored.insert(root);
    }
    if (next == std::string_view::npos) {
      break;
    }
    start = next + 1;
  }
  return ignored;
}

std::string first_rel_path_segment(const std::string& rel_path) {
  const std::size_t slash = rel_path.find('/');
  return slash == std::string::npos ? rel_path : rel_path.substr(0, slash);
}

void filter_ignored_roots(
    std::vector<kernel::storage::NoteCatalogRecord>& records,
    const std::set<std::string>& ignored_roots) {
  if (ignored_roots.empty()) {
    return;
  }

  records.erase(
      std::remove_if(
          records.begin(),
          records.end(),
          [&](const kernel::storage::NoteCatalogRecord& record) {
            return ignored_roots.contains(first_rel_path_segment(record.rel_path));
          }),
      records.end());
}

void copy_revision_to_note_record(std::string_view revision, kernel_note_record* out_note) {
  std::memset(out_note->content_revision, 0, sizeof(out_note->content_revision));
  const std::size_t count =
      std::min(revision.size(), static_cast<std::size_t>(KERNEL_REVISION_MAX - 1));
  std::memcpy(out_note->content_revision, revision.data(), count);
}

kernel_status fill_note_list(
    const std::vector<kernel::storage::NoteCatalogRecord>& records,
    kernel_note_list* out_notes) {
  if (records.empty()) {
    return kernel::core::make_status(KERNEL_OK);
  }

  auto* owned_notes = new (std::nothrow) kernel_note_record[records.size()];
  if (owned_notes == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }

  for (size_t index = 0; index < records.size(); ++index) {
    owned_notes[index].rel_path = nullptr;
    owned_notes[index].title = nullptr;
    owned_notes[index].file_size = 0;
    owned_notes[index].mtime_ns = 0;
    owned_notes[index].content_revision[0] = '\0';
  }

  out_notes->notes = owned_notes;
  out_notes->count = records.size();

  for (size_t index = 0; index < records.size(); ++index) {
    out_notes->notes[index].rel_path =
        kernel::core::duplicate_c_string(records[index].rel_path);
    out_notes->notes[index].title =
        kernel::core::duplicate_c_string(records[index].title);
    out_notes->notes[index].file_size = records[index].file_size;
    out_notes->notes[index].mtime_ns = records[index].mtime_ns;
    copy_revision_to_note_record(records[index].content_revision, &out_notes->notes[index]);

    if (out_notes->notes[index].rel_path == nullptr ||
        out_notes->notes[index].title == nullptr) {
      free_note_list_impl(out_notes);
      return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
    }
  }

  return kernel::core::make_status(KERNEL_OK);
}

}  // namespace

extern "C" kernel_status kernel_get_note_catalog_default_limit(std::size_t* out_limit) {
  if (out_limit == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  *out_limit = kDefaultNoteCatalogLimit;
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_get_vault_scan_default_limit(std::size_t* out_limit) {
  if (out_limit == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  *out_limit = kDefaultVaultScanLimit;
  return kernel::core::make_status(KERNEL_OK);
}

static kernel_status query_notes_impl(
    kernel_handle* handle,
    size_t limit,
    const char* ignored_roots_csv,
    kernel_note_list* out_notes) {
  reset_note_list(out_notes);
  if (handle == nullptr || out_notes == nullptr || limit == 0) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::vector<kernel::storage::NoteCatalogRecord> records;
  {
    std::lock_guard lock(handle->storage_mutex);
    const std::error_code ec =
        kernel::storage::list_note_catalog_records(handle->storage, limit, records);
    if (ec) {
      return kernel::core::make_status(kernel::core::map_error(ec));
    }
  }

  filter_ignored_roots(records, parse_ignored_roots(ignored_roots_csv));
  return fill_note_list(records, out_notes);
}

extern "C" kernel_status kernel_query_notes(
    kernel_handle* handle,
    size_t limit,
    kernel_note_list* out_notes) {
  return query_notes_impl(handle, limit, nullptr, out_notes);
}

extern "C" kernel_status kernel_query_notes_filtered(
    kernel_handle* handle,
    size_t limit,
    const char* ignored_roots_csv,
    kernel_note_list* out_notes) {
  return query_notes_impl(handle, limit, ignored_roots_csv, out_notes);
}

extern "C" void kernel_free_note_list(kernel_note_list* notes) {
  free_note_list_impl(notes);
}
