// Reason: This file centralizes the shared attachment-ABI marshalling helpers
// so the formal public surface and the legacy compatibility surface can live
// in separate implementation files without duplicating result handling.

#pragma once

#include "core/kernel_shared.h"
#include "kernel/c_api.h"
#include "storage/storage.h"

#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace kernel::core::attachment_api {

inline constexpr std::string_view kAttachmentPublicSurfaceRevision =
    "track2_batch1_public_surface_v1";
inline constexpr std::string_view kAttachmentMetadataContractRevision =
    "track2_batch2_metadata_contract_v1";
inline constexpr std::string_view kAttachmentKindMappingRevision =
    "track2_batch1_extension_mapping_v1";

kernel_status normalize_required_rel_path_argument(
    const char* rel_path,
    std::string& out_rel_path);

template <typename Result, typename QueryFn>
kernel_status run_locked_storage_query(
    kernel_handle* handle,
    Result& out_result,
    QueryFn&& query_fn) {
  std::lock_guard lock(handle->storage_mutex);
  const std::error_code ec =
      std::forward<QueryFn>(query_fn)(handle->storage, out_result);
  if (ec) {
    return kernel::core::make_status(kernel::core::map_error(ec));
  }
  return kernel::core::make_status(KERNEL_OK);
}

void reset_attachment_record(kernel_attachment_record* out_attachment);
void reset_attachment_list(kernel_attachment_list* out_attachments);
void reset_attachment_referrers(kernel_attachment_referrers* out_referrers);
void reset_attachment_refs(kernel_attachment_refs* out_refs);
void reset_attachment_metadata(kernel_attachment_metadata* out_metadata);

kernel_status fill_attachment_record(
    const kernel::storage::AttachmentCatalogRecord& record,
    kernel_attachment_record* out_attachment);
kernel_status fill_attachment_list(
    const std::vector<kernel::storage::AttachmentCatalogRecord>& records,
    kernel_attachment_list* out_attachments);
kernel_status fill_attachment_referrers(
    const std::vector<kernel::storage::AttachmentReferrerRecord>& referrers,
    kernel_attachment_referrers* out_referrers);
kernel_status fill_attachment_refs(
    const std::vector<std::string>& refs,
    kernel_attachment_refs* out_refs);
void fill_attachment_metadata(
    const kernel::storage::AttachmentMetadataRecord& metadata,
    kernel_attachment_metadata* out_metadata);

}  // namespace kernel::core::attachment_api
