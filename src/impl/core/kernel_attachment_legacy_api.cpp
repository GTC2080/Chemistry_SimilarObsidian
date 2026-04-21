// Reason: This file keeps the deprecated-but-supported minimal attachment ABI
// isolated from the formal Track 2 attachment public surface.

#include "kernel/c_api.h"

#include "core/kernel_attachment_api_shared.h"
#include "core/kernel_internal.h"
#include "core/kernel_shared.h"
#include "storage/storage.h"

#include <string>
#include <vector>

extern "C" kernel_status kernel_list_note_attachments(
    kernel_handle* handle,
    const char* note_rel_path,
    kernel_attachment_refs* out_refs) {
  kernel::core::attachment_api::reset_attachment_refs(out_refs);
  if (handle == nullptr || out_refs == nullptr ||
      !kernel::core::is_valid_relative_path(note_rel_path)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string normalized_note_rel_path =
      kernel::core::normalize_rel_path(note_rel_path);

  std::vector<std::string> refs;
  {
    std::lock_guard lock(handle->storage_mutex);
    const std::error_code ec = kernel::storage::list_note_attachment_refs(
        handle->storage,
        normalized_note_rel_path,
        refs);
    if (ec) {
      return kernel::core::make_status(kernel::core::map_error(ec));
    }
  }

  return kernel::core::attachment_api::fill_attachment_refs(refs, out_refs);
}

extern "C" kernel_status kernel_get_attachment_metadata(
    kernel_handle* handle,
    const char* attachment_rel_path,
    kernel_attachment_metadata* out_metadata) {
  if (out_metadata != nullptr) {
    out_metadata->file_size = 0;
    out_metadata->mtime_ns = 0;
    out_metadata->is_missing = 0;
  }

  if (handle == nullptr || out_metadata == nullptr ||
      !kernel::core::is_valid_relative_path(attachment_rel_path)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string normalized_attachment_rel_path =
      kernel::core::normalize_rel_path(attachment_rel_path);

  kernel::storage::AttachmentMetadataRecord metadata;
  {
    std::lock_guard lock(handle->storage_mutex);
    const std::error_code ec = kernel::storage::read_attachment_metadata(
        handle->storage,
        normalized_attachment_rel_path,
        metadata);
    if (ec) {
      return kernel::core::make_status(kernel::core::map_error(ec));
    }
  }

  out_metadata->file_size = metadata.file_size;
  out_metadata->mtime_ns = metadata.mtime_ns;
  out_metadata->is_missing = metadata.is_missing ? 1 : 0;
  return kernel::core::make_status(KERNEL_OK);
}
