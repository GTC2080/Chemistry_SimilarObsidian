// Reason: This file isolates PDF note-reference query shaping so Batch 3 can
// add a formal public surface without inflating metadata or attachment query units.

#pragma once

#include "core/kernel_internal.h"
#include "kernel/c_api.h"

#include <cstdint>
#include <string>
#include <vector>

namespace kernel::core::pdf_reference_query {

struct PdfSourceRefView {
  std::string pdf_rel_path;
  std::string anchor_serialized;
  std::string excerpt_text;
  std::uint64_t page = 0;
  kernel_pdf_ref_state state = KERNEL_PDF_REF_UNRESOLVED;
};

struct PdfReferrerView {
  std::string note_rel_path;
  std::string note_title;
  std::string anchor_serialized;
  std::string excerpt_text;
  std::uint64_t page = 0;
  kernel_pdf_ref_state state = KERNEL_PDF_REF_UNRESOLVED;
};

kernel_status query_note_pdf_source_refs(
    kernel_handle* handle,
    const char* note_rel_path,
    size_t limit,
    std::vector<PdfSourceRefView>& out_refs);
kernel_status query_pdf_referrers(
    kernel_handle* handle,
    const char* attachment_rel_path,
    size_t limit,
    std::vector<PdfReferrerView>& out_referrers);

}  // namespace kernel::core::pdf_reference_query
