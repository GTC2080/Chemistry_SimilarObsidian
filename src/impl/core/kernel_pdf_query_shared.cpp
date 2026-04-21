// Reason: This file owns PDF metadata storage-query plumbing so the public PDF
// ABI wrapper can stay focused on validation and result shaping.

#include "core/kernel_pdf_query_shared.h"

#include "core/kernel_attachment_api_shared.h"
#include "storage/storage.h"

#include <string>

namespace kernel::core::pdf_query {

kernel_status query_live_pdf_metadata_record(
    kernel_handle* handle,
    const char* attachment_rel_path,
    kernel::storage::PdfMetadataRecord& out_record) {
  std::string normalized_rel_path;
  const kernel_status normalized_status =
      kernel::core::attachment_api::normalize_required_rel_path_argument(
          attachment_rel_path,
          normalized_rel_path);
  if (normalized_status.code != KERNEL_OK) {
    return normalized_status;
  }

  return kernel::core::attachment_api::run_locked_storage_query(
      handle,
      out_record,
      [&](kernel::storage::Database& storage, auto& record) {
        return kernel::storage::read_live_pdf_metadata_record(
            storage,
            normalized_rel_path,
            record);
      });
}

}  // namespace kernel::core::pdf_query
