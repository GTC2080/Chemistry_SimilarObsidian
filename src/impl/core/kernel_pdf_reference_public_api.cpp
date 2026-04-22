// Reason: This file owns the formal Track 3 Batch 3 note↔PDF reference ABI so
// the new public surface stays separate from metadata-only PDF entry points.

#include "kernel/c_api.h"

#include "core/kernel_pdf_reference_api_shared.h"
#include "core/kernel_pdf_reference_query_shared.h"
#include "core/kernel_shared.h"

extern "C" kernel_status kernel_query_note_pdf_source_refs(
    kernel_handle* handle,
    const char* note_rel_path,
    const size_t limit,
    kernel_pdf_source_refs* out_refs) {
  kernel::core::pdf_reference_api::reset_pdf_source_refs(out_refs);
  if (handle == nullptr || out_refs == nullptr || limit == 0) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::vector<kernel::core::pdf_reference_query::PdfSourceRefView> refs;
  const kernel_status query_status =
      kernel::core::pdf_reference_query::query_note_pdf_source_refs(
          handle,
          note_rel_path,
          limit,
          refs);
  if (query_status.code != KERNEL_OK) {
    return query_status;
  }

  return kernel::core::pdf_reference_api::fill_pdf_source_refs(refs, out_refs);
}

extern "C" kernel_status kernel_query_pdf_referrers(
    kernel_handle* handle,
    const char* attachment_rel_path,
    const size_t limit,
    kernel_pdf_referrers* out_referrers) {
  kernel::core::pdf_reference_api::reset_pdf_referrers(out_referrers);
  if (handle == nullptr || out_referrers == nullptr || limit == 0) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::vector<kernel::core::pdf_reference_query::PdfReferrerView> referrers;
  const kernel_status query_status =
      kernel::core::pdf_reference_query::query_pdf_referrers(
          handle,
          attachment_rel_path,
          limit,
          referrers);
  if (query_status.code != KERNEL_OK) {
    return query_status;
  }

  return kernel::core::pdf_reference_api::fill_pdf_referrers(
      referrers,
      out_referrers);
}
