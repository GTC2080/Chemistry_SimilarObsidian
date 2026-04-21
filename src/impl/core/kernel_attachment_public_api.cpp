// Reason: This file owns the formal Track 2 attachment public surface so the
// live-catalog attachment contract stays separate from legacy compatibility APIs.

#include "kernel/c_api.h"

#include "core/kernel_attachment_api_shared.h"
#include "core/kernel_internal.h"
#include "core/kernel_shared.h"
#include "storage/storage.h"

#include <string>
#include <vector>

extern "C" kernel_status kernel_query_attachments(
    kernel_handle* handle,
    const size_t limit,
    kernel_attachment_list* out_attachments) {
  kernel::core::attachment_api::reset_attachment_list(out_attachments);
  if (handle == nullptr || out_attachments == nullptr || limit == 0) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::vector<kernel::storage::AttachmentCatalogRecord> records;
  {
    std::lock_guard lock(handle->storage_mutex);
    const std::error_code ec =
        kernel::storage::list_live_attachment_records(handle->storage, limit, records);
    if (ec) {
      return kernel::core::make_status(kernel::core::map_error(ec));
    }
  }

  return kernel::core::attachment_api::fill_attachment_list(records, out_attachments);
}

extern "C" kernel_status kernel_get_attachment(
    kernel_handle* handle,
    const char* attachment_rel_path,
    kernel_attachment_record* out_attachment) {
  kernel::core::attachment_api::reset_attachment_record(out_attachment);
  if (handle == nullptr || out_attachment == nullptr ||
      !kernel::core::is_valid_relative_path(attachment_rel_path)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string normalized_attachment_rel_path =
      kernel::core::normalize_rel_path(attachment_rel_path);

  kernel::storage::AttachmentCatalogRecord record;
  {
    std::lock_guard lock(handle->storage_mutex);
    const std::error_code ec = kernel::storage::read_live_attachment_record(
        handle->storage,
        normalized_attachment_rel_path,
        record);
    if (ec) {
      return kernel::core::make_status(kernel::core::map_error(ec));
    }
  }

  return kernel::core::attachment_api::fill_attachment_record(record, out_attachment);
}

extern "C" kernel_status kernel_query_note_attachment_refs(
    kernel_handle* handle,
    const char* note_rel_path,
    const size_t limit,
    kernel_attachment_list* out_attachments) {
  kernel::core::attachment_api::reset_attachment_list(out_attachments);
  if (handle == nullptr || out_attachments == nullptr || limit == 0 ||
      !kernel::core::is_valid_relative_path(note_rel_path)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string normalized_note_rel_path =
      kernel::core::normalize_rel_path(note_rel_path);

  std::vector<kernel::storage::AttachmentCatalogRecord> records;
  {
    std::lock_guard lock(handle->storage_mutex);
    const std::error_code ec = kernel::storage::list_note_attachment_records(
        handle->storage,
        normalized_note_rel_path,
        limit,
        records);
    if (ec) {
      return kernel::core::make_status(kernel::core::map_error(ec));
    }
  }

  return kernel::core::attachment_api::fill_attachment_list(records, out_attachments);
}

extern "C" kernel_status kernel_query_attachment_referrers(
    kernel_handle* handle,
    const char* attachment_rel_path,
    const size_t limit,
    kernel_attachment_referrers* out_referrers) {
  kernel::core::attachment_api::reset_attachment_referrers(out_referrers);
  if (handle == nullptr || out_referrers == nullptr || limit == 0 ||
      !kernel::core::is_valid_relative_path(attachment_rel_path)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string normalized_attachment_rel_path =
      kernel::core::normalize_rel_path(attachment_rel_path);

  std::vector<kernel::storage::AttachmentReferrerRecord> referrers;
  {
    std::lock_guard lock(handle->storage_mutex);
    const std::error_code ec = kernel::storage::list_attachment_referrers(
        handle->storage,
        normalized_attachment_rel_path,
        limit,
        referrers);
    if (ec) {
      return kernel::core::make_status(kernel::core::map_error(ec));
    }
  }

  return kernel::core::attachment_api::fill_attachment_referrers(referrers, out_referrers);
}
