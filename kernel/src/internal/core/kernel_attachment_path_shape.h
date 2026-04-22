// Reason: This file isolates attachment path-shape derivation so ABI
// marshalling code does not also own extension mapping and kind classification.

#pragma once

#include "kernel/c_api.h"

#include <string>
#include <string_view>

namespace kernel::core::attachment_path_shape {

struct AttachmentPathShape {
  std::string basename;
  std::string extension;
  kernel_attachment_kind kind = KERNEL_ATTACHMENT_KIND_UNKNOWN;
};

AttachmentPathShape describe_attachment_path(std::string_view rel_path);

}  // namespace kernel::core::attachment_path_shape
