// Reason: This file keeps PDF metadata storage-query plumbing separate from
// ABI marshalling so the public PDF surface remains small and focused.

#pragma once

#include "core/kernel_internal.h"
#include "pdf/pdf_anchor.h"
#include "storage/storage.h"

#include <vector>

namespace kernel::core::pdf_query {

kernel_status query_live_pdf_metadata_record(
    kernel_handle* handle,
    const char* attachment_rel_path,
    kernel::storage::PdfMetadataRecord& out_record);
kernel_status query_live_pdf_anchor_records(
    kernel_handle* handle,
    const char* attachment_rel_path,
    std::vector<kernel::storage::PdfAnchorRecord>& out_records);
kernel_status validate_live_pdf_anchor(
    kernel_handle* handle,
    std::string_view serialized_anchor,
    kernel::pdf::PdfAnchorValidationResult& out_result);

}  // namespace kernel::core::pdf_query
