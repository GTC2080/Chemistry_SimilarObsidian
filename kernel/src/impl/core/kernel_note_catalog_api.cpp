// Reason: This file owns the host-facing note catalog query surface.

#include "kernel/c_api.h"

#include "core/kernel_internal.h"
#include "core/kernel_shared.h"
#include "storage/storage.h"

#include <algorithm>
#include <cstring>
#include <new>
#include <vector>

namespace {

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

extern "C" kernel_status kernel_query_notes(
    kernel_handle* handle,
    size_t limit,
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

  return fill_note_list(records, out_notes);
}

extern "C" void kernel_free_note_list(kernel_note_list* notes) {
  free_note_list_impl(notes);
}
