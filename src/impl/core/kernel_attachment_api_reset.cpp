// Reason: This file owns attachment ABI reset/free helpers so marshalling and
// query wrappers do not also carry cleanup logic.

#include "core/kernel_attachment_api_shared.h"

namespace {

void free_attachment_record_impl(kernel_attachment_record* attachment) {
  if (attachment == nullptr) {
    return;
  }

  delete[] attachment->rel_path;
  delete[] attachment->basename;
  delete[] attachment->extension;
  attachment->rel_path = nullptr;
  attachment->basename = nullptr;
  attachment->extension = nullptr;
  attachment->file_size = 0;
  attachment->mtime_ns = 0;
  attachment->ref_count = 0;
  attachment->kind = KERNEL_ATTACHMENT_KIND_UNKNOWN;
  attachment->flags = KERNEL_ATTACHMENT_FLAG_NONE;
  attachment->presence = KERNEL_ATTACHMENT_PRESENCE_PRESENT;
}

}  // namespace

namespace kernel::core::attachment_api {

void reset_attachment_record(kernel_attachment_record* out_attachment) {
  free_attachment_record_impl(out_attachment);
}

void reset_attachment_list(kernel_attachment_list* out_attachments) {
  if (out_attachments == nullptr) {
    return;
  }

  if (out_attachments->attachments != nullptr) {
    for (size_t index = 0; index < out_attachments->count; ++index) {
      free_attachment_record_impl(&out_attachments->attachments[index]);
    }
    delete[] out_attachments->attachments;
  }

  out_attachments->attachments = nullptr;
  out_attachments->count = 0;
}

void reset_attachment_referrers(kernel_attachment_referrers* out_referrers) {
  if (out_referrers == nullptr) {
    return;
  }

  if (out_referrers->referrers != nullptr) {
    for (size_t index = 0; index < out_referrers->count; ++index) {
      delete[] out_referrers->referrers[index].note_rel_path;
      delete[] out_referrers->referrers[index].note_title;
    }
    delete[] out_referrers->referrers;
  }

  out_referrers->referrers = nullptr;
  out_referrers->count = 0;
}

void reset_attachment_refs(kernel_attachment_refs* out_refs) {
  if (out_refs == nullptr) {
    return;
  }

  if (out_refs->refs != nullptr) {
    for (size_t index = 0; index < out_refs->count; ++index) {
      delete[] out_refs->refs[index].rel_path;
    }
    delete[] out_refs->refs;
  }

  out_refs->refs = nullptr;
  out_refs->count = 0;
}

void reset_attachment_metadata(kernel_attachment_metadata* out_metadata) {
  if (out_metadata == nullptr) {
    return;
  }

  out_metadata->file_size = 0;
  out_metadata->mtime_ns = 0;
  out_metadata->is_missing = 0;
}

}  // namespace kernel::core::attachment_api

extern "C" void kernel_free_attachment_record(kernel_attachment_record* attachment) {
  kernel::core::attachment_api::reset_attachment_record(attachment);
}

extern "C" void kernel_free_attachment_list(kernel_attachment_list* attachments) {
  kernel::core::attachment_api::reset_attachment_list(attachments);
}

extern "C" void kernel_free_attachment_referrers(kernel_attachment_referrers* referrers) {
  kernel::core::attachment_api::reset_attachment_referrers(referrers);
}

extern "C" void kernel_free_attachment_refs(kernel_attachment_refs* refs) {
  kernel::core::attachment_api::reset_attachment_refs(refs);
}
