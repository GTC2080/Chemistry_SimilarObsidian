// Reason: This file centralizes attachment storage-query helpers so the
// formal public surface and the legacy compatibility surface can stay thin.

#pragma once

#include "core/kernel_internal.h"
#include "kernel/c_api.h"
#include "storage/storage.h"

#include <string>
#include <vector>

namespace kernel::core::attachment_query {

kernel_status query_live_attachment_list(
    kernel_handle* handle,
    size_t limit,
    std::vector<kernel::storage::AttachmentCatalogRecord>& out_records);

kernel_status query_live_attachment_record(
    kernel_handle* handle,
    const char* attachment_rel_path,
    kernel::storage::AttachmentCatalogRecord& out_record);

kernel_status query_note_attachment_records(
    kernel_handle* handle,
    const char* note_rel_path,
    size_t limit,
    std::vector<kernel::storage::AttachmentCatalogRecord>& out_records);

kernel_status query_attachment_referrers(
    kernel_handle* handle,
    const char* attachment_rel_path,
    size_t limit,
    std::vector<kernel::storage::AttachmentReferrerRecord>& out_referrers);

kernel_status query_legacy_note_attachment_refs(
    kernel_handle* handle,
    const char* note_rel_path,
    std::vector<std::string>& out_refs);

kernel_status query_legacy_attachment_metadata(
    kernel_handle* handle,
    const char* attachment_rel_path,
    kernel::storage::AttachmentMetadataRecord& out_metadata);

}  // namespace kernel::core::attachment_query
