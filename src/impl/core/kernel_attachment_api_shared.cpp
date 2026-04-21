// Reason: This file keeps attachment ABI normalization tiny so cleanup and
// result marshalling can live in focused implementation units.

#include "core/kernel_attachment_api_shared.h"

namespace kernel::core::attachment_api {

kernel_status normalize_required_rel_path_argument(
    const char* rel_path,
    std::string& out_rel_path) {
  out_rel_path.clear();
  if (!kernel::core::is_valid_relative_path(rel_path)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  out_rel_path = kernel::core::normalize_rel_path(rel_path);
  return kernel::core::make_status(KERNEL_OK);
}

}  // namespace kernel::core::attachment_api
