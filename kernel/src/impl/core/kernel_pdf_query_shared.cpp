// Reason: This file owns PDF metadata storage-query plumbing so the public PDF
// ABI wrapper can stay focused on validation and result shaping.

#include "core/kernel_pdf_query_shared.h"

#include "core/kernel_attachment_api_shared.h"
#include "storage/storage.h"

#include <string>
#include <vector>

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

kernel_status query_live_pdf_anchor_records(
    kernel_handle* handle,
    const char* attachment_rel_path,
    std::vector<kernel::storage::PdfAnchorRecord>& out_records) {
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
      out_records,
      [&](kernel::storage::Database& storage, auto& records) {
        return kernel::storage::list_live_pdf_anchor_records(
            storage,
            normalized_rel_path,
            records);
      });
}

kernel_status validate_live_pdf_anchor(
    kernel_handle* handle,
    std::string_view serialized_anchor,
    kernel::pdf::PdfAnchorValidationResult& out_result) {
  kernel::pdf::ParsedPdfAnchor parsed_anchor;
  if (!kernel::pdf::parse_pdf_anchor(serialized_anchor, parsed_anchor)) {
    out_result = kernel::pdf::PdfAnchorValidationResult{};
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  return kernel::core::attachment_api::run_locked_storage_query(
      handle,
      out_result,
      [&](kernel::storage::Database& storage, auto& result) {
        kernel::storage::PdfMetadataRecord metadata;
        kernel::storage::PdfAnchorRecord anchor;
        const std::error_code metadata_ec =
            kernel::storage::read_live_pdf_metadata_record(
                storage,
                parsed_anchor.rel_path,
                metadata);
        if (!metadata_ec) {
          const std::error_code anchor_ec =
              kernel::storage::read_live_pdf_anchor_record(
                  storage,
                  parsed_anchor.rel_path,
                  parsed_anchor.page,
                  anchor);
          result = kernel::pdf::validate_pdf_anchor(
              serialized_anchor,
              &metadata,
              anchor_ec ? nullptr : &anchor);
          return std::error_code{};
        }

        if (metadata_ec != std::make_error_code(std::errc::no_such_file_or_directory)) {
          return metadata_ec;
        }

        result = kernel::pdf::validate_pdf_anchor(serialized_anchor, nullptr, nullptr);
        return std::error_code{};
      });
}

}  // namespace kernel::core::pdf_query
