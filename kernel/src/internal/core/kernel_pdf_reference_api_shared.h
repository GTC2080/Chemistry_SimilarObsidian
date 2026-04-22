// Reason: This file centralizes PDF note-reference ABI helpers so the formal
// Track 3 Batch 3 public surface can stay thin and avoid bloating metadata-only units.

#pragma once

#include "core/kernel_pdf_reference_query_shared.h"
#include "kernel/c_api.h"

#include <vector>

namespace kernel::core::pdf_reference_api {

void reset_pdf_source_refs(kernel_pdf_source_refs* out_refs);
void reset_pdf_referrers(kernel_pdf_referrers* out_referrers);

kernel_status fill_pdf_source_refs(
    const std::vector<kernel::core::pdf_reference_query::PdfSourceRefView>& refs,
    kernel_pdf_source_refs* out_refs);
kernel_status fill_pdf_referrers(
    const std::vector<kernel::core::pdf_reference_query::PdfReferrerView>& referrers,
    kernel_pdf_referrers* out_referrers);

}  // namespace kernel::core::pdf_reference_api
