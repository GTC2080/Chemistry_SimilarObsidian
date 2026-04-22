// Reason: This file isolates PDF reference-state resolution so public query
// surfaces and diagnostics export can share one frozen validation path.

#pragma once

#include "kernel/c_api.h"
#include "storage/storage.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>

namespace kernel::core::pdf_reference_resolution {

struct ResolvedPdfReference {
  std::string excerpt_text;
  std::uint64_t page = 0;
  kernel_pdf_ref_state state = KERNEL_PDF_REF_UNRESOLVED;
};

std::error_code resolve_pdf_reference(
    kernel::storage::Database& storage,
    std::string_view pdf_rel_path,
    std::string_view anchor_serialized,
    std::uint64_t stored_page,
    std::string_view stored_excerpt_text,
    ResolvedPdfReference& out_reference);

}  // namespace kernel::core::pdf_reference_resolution
