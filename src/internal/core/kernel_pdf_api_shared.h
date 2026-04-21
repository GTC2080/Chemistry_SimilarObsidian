// Reason: This file centralizes the narrow PDF metadata ABI helpers so the
// public PDF surface can stay thin and avoid bloating attachment-facing files.

#pragma once

#include "kernel/c_api.h"
#include "storage/storage.h"

namespace kernel::core::pdf_api {

void reset_pdf_metadata_record(kernel_pdf_metadata_record* out_metadata);
kernel_status fill_pdf_metadata_record(
    const kernel::storage::PdfMetadataRecord& record,
    kernel_pdf_metadata_record* out_metadata);

}  // namespace kernel::core::pdf_api
