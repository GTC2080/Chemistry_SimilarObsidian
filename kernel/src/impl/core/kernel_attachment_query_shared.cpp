// Reason: This file owns attachment storage-query plumbing so ABI wrappers can
// stay focused on contract validation and result marshalling.

#include "core/kernel_attachment_query_shared.h"

#include "core/kernel_attachment_api_shared.h"
#include "storage/storage.h"

#include <string>
#include <vector>

namespace {

template <typename Result, typename QueryFn>
kernel_status run_normalized_rel_path_storage_query(
    kernel_handle* handle,
    const char* rel_path,
    Result& out_result,
    QueryFn&& query_fn) {
  std::string normalized_rel_path;
  const kernel_status normalized_status =
      kernel::core::attachment_api::normalize_required_rel_path_argument(
          rel_path,
          normalized_rel_path);
  if (normalized_status.code != KERNEL_OK) {
    return normalized_status;
  }

  return kernel::core::attachment_api::run_locked_storage_query(
      handle,
      out_result,
      [&](kernel::storage::Database& storage, auto& query_result) {
        return std::forward<QueryFn>(query_fn)(
            storage,
            normalized_rel_path,
            query_result);
      });
}

}  // namespace

namespace kernel::core::attachment_query {

kernel_status query_live_attachment_list(
    kernel_handle* handle,
    const size_t limit,
    std::vector<kernel::storage::AttachmentCatalogRecord>& out_records) {
  return kernel::core::attachment_api::run_locked_storage_query(
      handle,
      out_records,
      [&](kernel::storage::Database& storage, auto& records) {
        return kernel::storage::list_live_attachment_records(
            storage,
            limit,
            records);
      });
}

kernel_status query_live_attachment_record(
    kernel_handle* handle,
    const char* attachment_rel_path,
    kernel::storage::AttachmentCatalogRecord& out_record) {
  return run_normalized_rel_path_storage_query(
      handle,
      attachment_rel_path,
      out_record,
      [](kernel::storage::Database& storage,
         const std::string& normalized_rel_path,
         auto& record) {
        return kernel::storage::read_live_attachment_record(
            storage,
            normalized_rel_path,
            record);
      });
}

kernel_status query_note_attachment_records(
    kernel_handle* handle,
    const char* note_rel_path,
    const size_t limit,
    std::vector<kernel::storage::AttachmentCatalogRecord>& out_records) {
  return run_normalized_rel_path_storage_query(
      handle,
      note_rel_path,
      out_records,
      [&](kernel::storage::Database& storage,
          const std::string& normalized_rel_path,
          auto& records) {
        return kernel::storage::list_note_attachment_records(
            storage,
            normalized_rel_path,
            limit,
            records);
      });
}

kernel_status query_attachment_referrers(
    kernel_handle* handle,
    const char* attachment_rel_path,
    const size_t limit,
    std::vector<kernel::storage::AttachmentReferrerRecord>& out_referrers) {
  return run_normalized_rel_path_storage_query(
      handle,
      attachment_rel_path,
      out_referrers,
      [&](kernel::storage::Database& storage,
          const std::string& normalized_rel_path,
          auto& referrers) {
        return kernel::storage::list_attachment_referrers(
            storage,
            normalized_rel_path,
            limit,
            referrers);
      });
}

kernel_status query_legacy_note_attachment_refs(
    kernel_handle* handle,
    const char* note_rel_path,
    std::vector<std::string>& out_refs) {
  return run_normalized_rel_path_storage_query(
      handle,
      note_rel_path,
      out_refs,
      [](kernel::storage::Database& storage,
         const std::string& normalized_rel_path,
         auto& refs) {
        return kernel::storage::list_note_attachment_refs(
            storage,
            normalized_rel_path,
            refs);
      });
}

kernel_status query_legacy_attachment_metadata(
    kernel_handle* handle,
    const char* attachment_rel_path,
    kernel::storage::AttachmentMetadataRecord& out_metadata) {
  return run_normalized_rel_path_storage_query(
      handle,
      attachment_rel_path,
      out_metadata,
      [](kernel::storage::Database& storage,
         const std::string& normalized_rel_path,
         auto& metadata) {
        return kernel::storage::read_attachment_metadata(
            storage,
            normalized_rel_path,
            metadata);
      });
}

}  // namespace kernel::core::attachment_query
