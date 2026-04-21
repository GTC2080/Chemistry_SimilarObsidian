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
  const kernel_status query_status =
      kernel::core::attachment_api::run_locked_storage_query(
          handle,
          records,
          [&](kernel::storage::Database& storage, auto& out_records) {
            return kernel::storage::list_live_attachment_records(
                storage,
                limit,
                out_records);
          });
  if (query_status.code != KERNEL_OK) {
    return query_status;
  }

  return kernel::core::attachment_api::fill_attachment_list(records, out_attachments);
}

extern "C" kernel_status kernel_get_attachment(
    kernel_handle* handle,
    const char* attachment_rel_path,
    kernel_attachment_record* out_attachment) {
  kernel::core::attachment_api::reset_attachment_record(out_attachment);
  if (handle == nullptr || out_attachment == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::string normalized_attachment_rel_path;
  const kernel_status normalized_status =
      kernel::core::attachment_api::normalize_required_rel_path_argument(
          attachment_rel_path,
          normalized_attachment_rel_path);
  if (normalized_status.code != KERNEL_OK) {
    return normalized_status;
  }

  kernel::storage::AttachmentCatalogRecord record;
  const kernel_status query_status =
      kernel::core::attachment_api::run_locked_storage_query(
          handle,
          record,
          [&](kernel::storage::Database& storage, auto& out_record) {
            return kernel::storage::read_live_attachment_record(
                storage,
                normalized_attachment_rel_path,
                out_record);
          });
  if (query_status.code != KERNEL_OK) {
    return query_status;
  }

  return kernel::core::attachment_api::fill_attachment_record(record, out_attachment);
}

extern "C" kernel_status kernel_query_note_attachment_refs(
    kernel_handle* handle,
    const char* note_rel_path,
    const size_t limit,
    kernel_attachment_list* out_attachments) {
  kernel::core::attachment_api::reset_attachment_list(out_attachments);
  if (handle == nullptr || out_attachments == nullptr || limit == 0) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::string normalized_note_rel_path;
  const kernel_status normalized_status =
      kernel::core::attachment_api::normalize_required_rel_path_argument(
          note_rel_path,
          normalized_note_rel_path);
  if (normalized_status.code != KERNEL_OK) {
    return normalized_status;
  }

  std::vector<kernel::storage::AttachmentCatalogRecord> records;
  const kernel_status query_status =
      kernel::core::attachment_api::run_locked_storage_query(
          handle,
          records,
          [&](kernel::storage::Database& storage, auto& out_records) {
            return kernel::storage::list_note_attachment_records(
                storage,
                normalized_note_rel_path,
                limit,
                out_records);
          });
  if (query_status.code != KERNEL_OK) {
    return query_status;
  }

  return kernel::core::attachment_api::fill_attachment_list(records, out_attachments);
}

extern "C" kernel_status kernel_query_attachment_referrers(
    kernel_handle* handle,
    const char* attachment_rel_path,
    const size_t limit,
    kernel_attachment_referrers* out_referrers) {
  kernel::core::attachment_api::reset_attachment_referrers(out_referrers);
  if (handle == nullptr || out_referrers == nullptr || limit == 0) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::string normalized_attachment_rel_path;
  const kernel_status normalized_status =
      kernel::core::attachment_api::normalize_required_rel_path_argument(
          attachment_rel_path,
          normalized_attachment_rel_path);
  if (normalized_status.code != KERNEL_OK) {
    return normalized_status;
  }

  std::vector<kernel::storage::AttachmentReferrerRecord> referrers;
  const kernel_status query_status =
      kernel::core::attachment_api::run_locked_storage_query(
          handle,
          referrers,
          [&](kernel::storage::Database& storage, auto& out_referrers) {
            return kernel::storage::list_attachment_referrers(
                storage,
                normalized_attachment_rel_path,
                limit,
                out_referrers);
          });
  if (query_status.code != KERNEL_OK) {
    return query_status;
  }

  return kernel::core::attachment_api::fill_attachment_referrers(referrers, out_referrers);
}
