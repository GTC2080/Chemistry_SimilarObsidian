// Reason: This file keeps the deprecated-but-supported minimal attachment ABI
// isolated from the formal Track 2 attachment public surface.

#include "kernel/c_api.h"

#include "core/kernel_attachment_api_shared.h"
#include "core/kernel_attachment_query_shared.h"
#include "core/kernel_internal.h"
#include "core/kernel_shared.h"

#include <vector>

extern "C" kernel_status kernel_list_note_attachments(
    kernel_handle* handle,
    const char* note_rel_path,
    kernel_attachment_refs* out_refs) {
  kernel::core::attachment_api::reset_attachment_refs(out_refs);
  if (handle == nullptr || out_refs == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::vector<std::string> refs;
  const kernel_status query_status =
      kernel::core::attachment_query::query_legacy_note_attachment_refs(
          handle,
          note_rel_path,
          refs);
  if (query_status.code != KERNEL_OK) {
    return query_status;
  }

  return kernel::core::attachment_api::fill_attachment_refs(refs, out_refs);
}

extern "C" kernel_status kernel_get_attachment_metadata(
    kernel_handle* handle,
    const char* attachment_rel_path,
    kernel_attachment_metadata* out_metadata) {
  kernel::core::attachment_api::reset_attachment_metadata(out_metadata);

  if (handle == nullptr || out_metadata == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel::storage::AttachmentMetadataRecord metadata;
  const kernel_status query_status =
      kernel::core::attachment_query::query_legacy_attachment_metadata(
          handle,
          attachment_rel_path,
          metadata);
  if (query_status.code != KERNEL_OK) {
    return query_status;
  }

  kernel::core::attachment_api::fill_attachment_metadata(metadata, out_metadata);
  return kernel::core::make_status(KERNEL_OK);
}
