// Reason: This file owns the shared attachment-ABI marshalling helpers so
// formal Track 2 surfaces and legacy compatibility entry points can stay split.

#include "core/kernel_attachment_api_shared.h"

#include "core/kernel_shared.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <new>
#include <string>

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

struct AttachmentPathShape {
  std::string basename;
  std::string extension;
  kernel_attachment_kind kind = KERNEL_ATTACHMENT_KIND_UNKNOWN;
};

std::string to_lower_ascii(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
}

std::string attachment_basename_from_rel_path(std::string_view rel_path) {
  const std::filesystem::path path(rel_path);
  const std::filesystem::path filename = path.filename();
  if (filename.empty()) {
    return std::string(rel_path);
  }
  return filename.generic_string();
}

kernel_attachment_kind classify_attachment_kind(const std::string_view extension) {
  if (extension.empty()) {
    return KERNEL_ATTACHMENT_KIND_UNKNOWN;
  }

  if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" ||
      extension == ".gif" || extension == ".bmp" || extension == ".webp" ||
      extension == ".svg" || extension == ".tif" || extension == ".tiff") {
    return KERNEL_ATTACHMENT_KIND_IMAGE_LIKE;
  }
  if (extension == ".pdf") {
    return KERNEL_ATTACHMENT_KIND_PDF_LIKE;
  }
  if (extension == ".mol" || extension == ".mol2" || extension == ".sdf" ||
      extension == ".sd" || extension == ".pdb" || extension == ".cif" ||
      extension == ".xyz" || extension == ".cdx" || extension == ".cdxml" ||
      extension == ".rxn") {
    return KERNEL_ATTACHMENT_KIND_CHEM_LIKE;
  }
  return KERNEL_ATTACHMENT_KIND_GENERIC_FILE;
}

AttachmentPathShape describe_attachment_path(std::string_view rel_path) {
  const std::filesystem::path path(rel_path);
  AttachmentPathShape shape{};
  shape.basename = attachment_basename_from_rel_path(rel_path);
  shape.extension = to_lower_ascii(path.extension().generic_string());
  shape.kind = classify_attachment_kind(shape.extension);
  return shape;
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

kernel_status fill_attachment_record(
    const kernel::storage::AttachmentCatalogRecord& record,
    kernel_attachment_record* out_attachment) {
  const AttachmentPathShape path_shape = describe_attachment_path(record.rel_path);

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
    free_attachment_record_impl(out_attachment);
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
