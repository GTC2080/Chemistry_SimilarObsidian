// Reason: This file keeps PDF metadata storage-query plumbing separate from
// ABI marshalling so the public PDF surface remains small and focused.

#pragma once

#include "core/kernel_internal.h"
#include "storage/storage.h"

namespace kernel::core::pdf_query {

kernel_status query_live_pdf_metadata_record(
    kernel_handle* handle,
    const char* attachment_rel_path,
    kernel::storage::PdfMetadataRecord& out_record);

}  // namespace kernel::core::pdf_query
