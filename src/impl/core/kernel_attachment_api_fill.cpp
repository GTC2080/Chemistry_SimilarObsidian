// Reason: This file owns attachment ABI result marshalling so cleanup and
// normalization logic can live in smaller dedicated units.

#include "core/kernel_attachment_api_shared.h"
#include "core/kernel_attachment_path_shape.h"

#include "core/kernel_shared.h"

#include <new>
#include <string>

namespace kernel::core::attachment_api {

kernel_status fill_attachment_record(
    const kernel::storage::AttachmentCatalogRecord& record,
    kernel_attachment_record* out_attachment) {
  const auto path_shape =
      kernel::core::attachment_path_shape::describe_attachment_path(record.rel_path);

  out_attachment->rel_path = kernel::core::duplicate_c_string(record.rel_path);
  out_attachment->basename = kernel::core::duplicate_c_string(path_shape.basename);
  out_attachment->extension = kernel::core::duplicate_c_string(path_shape.extension);
  out_attachment->file_size = record.file_size;
  out_attachment->mtime_ns = record.mtime_ns;
  out_attachment->ref_count = record.ref_count;
  out_attachment->kind = path_shape.kind;
  out_attachment->flags = KERNEL_ATTACHMENT_FLAG_NONE;
  out_attachment->presence = record.is_missing ? KERNEL_ATTACHMENT_PRESENCE_MISSING
                                               : KERNEL_ATTACHMENT_PRESENCE_PRESENT;
  if (out_attachment->rel_path == nullptr || out_attachment->basename == nullptr ||
      out_attachment->extension == nullptr) {
    reset_attachment_record(out_attachment);
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }

  return kernel::core::make_status(KERNEL_OK);
}

kernel_status fill_attachment_list(
    const std::vector<kernel::storage::AttachmentCatalogRecord>& records,
    kernel_attachment_list* out_attachments) {
  if (records.empty()) {
    return kernel::core::make_status(KERNEL_OK);
  }

  auto* owned_attachments = new (std::nothrow) kernel_attachment_record[records.size()];
  if (owned_attachments == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }

  for (size_t index = 0; index < records.size(); ++index) {
    owned_attachments[index] = kernel_attachment_record{};
  }

  out_attachments->attachments = owned_attachments;
  out_attachments->count = records.size();
  for (size_t index = 0; index < records.size(); ++index) {
    const kernel_status status = fill_attachment_record(records[index], &owned_attachments[index]);
    if (status.code != KERNEL_OK) {
      reset_attachment_list(out_attachments);
      return status;
    }
  }

  return kernel::core::make_status(KERNEL_OK);
}

kernel_status fill_attachment_referrers(
    const std::vector<kernel::storage::AttachmentReferrerRecord>& referrers,
    kernel_attachment_referrers* out_referrers) {
  if (referrers.empty()) {
    return kernel::core::make_status(KERNEL_OK);
  }

  auto* owned_referrers = new (std::nothrow) kernel_attachment_referrer[referrers.size()];
  if (owned_referrers == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }

  for (size_t index = 0; index < referrers.size(); ++index) {
    owned_referrers[index].note_rel_path = nullptr;
    owned_referrers[index].note_title = nullptr;
  }

  out_referrers->referrers = owned_referrers;
  out_referrers->count = referrers.size();
  for (size_t index = 0; index < referrers.size(); ++index) {
    owned_referrers[index].note_rel_path =
        kernel::core::duplicate_c_string(referrers[index].note_rel_path);
    owned_referrers[index].note_title =
        kernel::core::duplicate_c_string(referrers[index].note_title);
    if (owned_referrers[index].note_rel_path == nullptr ||
        owned_referrers[index].note_title == nullptr) {
      reset_attachment_referrers(out_referrers);
      return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
    }
  }

  return kernel::core::make_status(KERNEL_OK);
}

kernel_status fill_attachment_refs(
    const std::vector<std::string>& refs,
    kernel_attachment_refs* out_refs) {
  if (refs.empty()) {
    return kernel::core::make_status(KERNEL_OK);
  }

  auto* owned_refs = new (std::nothrow) kernel_attachment_ref[refs.size()];
  if (owned_refs == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }

  for (size_t index = 0; index < refs.size(); ++index) {
    owned_refs[index].rel_path = nullptr;
  }

  out_refs->refs = owned_refs;
  out_refs->count = refs.size();
  for (size_t index = 0; index < refs.size(); ++index) {
    owned_refs[index].rel_path = kernel::core::duplicate_c_string(refs[index]);
    if (owned_refs[index].rel_path == nullptr) {
      reset_attachment_refs(out_refs);
      return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
    }
  }

  return kernel::core::make_status(KERNEL_OK);
}

void fill_attachment_metadata(
    const kernel::storage::AttachmentMetadataRecord& metadata,
    kernel_attachment_metadata* out_metadata) {
  out_metadata->file_size = metadata.file_size;
  out_metadata->mtime_ns = metadata.mtime_ns;
  out_metadata->is_missing = metadata.is_missing ? 1 : 0;
}

}  // namespace kernel::core::attachment_api
