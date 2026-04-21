// Reason: This file owns attachment basename/extension/kind derivation so
// attachment ABI marshalling can stay focused on result shaping.

#include "core/kernel_attachment_path_shape.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <string_view>

namespace {

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

}  // namespace

namespace kernel::core::attachment_path_shape {

AttachmentPathShape describe_attachment_path(std::string_view rel_path) {
  const std::filesystem::path path(rel_path);
  AttachmentPathShape shape{};
  shape.basename = attachment_basename_from_rel_path(rel_path);
  shape.extension = to_lower_ascii(path.extension().generic_string());
  shape.kind = classify_attachment_kind(shape.extension);
  return shape;
}

}  // namespace kernel::core::attachment_path_shape
