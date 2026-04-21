// Reason: This file centralizes the shared attachment-ABI marshalling helpers
// so the formal public surface and the legacy compatibility surface can live
// in separate implementation files without duplicating result handling.

#pragma once

#include "kernel/c_api.h"
#include "storage/storage.h"

#include <string>
#include <vector>

namespace kernel::core::attachment_api {

void reset_attachment_record(kernel_attachment_record* out_attachment);
void reset_attachment_list(kernel_attachment_list* out_attachments);
void reset_attachment_referrers(kernel_attachment_referrers* out_referrers);
void reset_attachment_refs(kernel_attachment_refs* out_refs);

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

}  // namespace kernel::core::attachment_api
