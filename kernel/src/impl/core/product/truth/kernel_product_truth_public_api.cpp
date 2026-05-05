// Reason: Expose truth product rules through focused C ABI wrappers.

#include "kernel/c_api.h"

#include "core/kernel_product_truth.h"
#include "core/kernel_shared.h"

#include <string_view>

extern "C" kernel_status kernel_compute_truth_diff(
    const char* prev_content,
    const std::size_t prev_size,
    const char* curr_content,
    const std::size_t curr_size,
    const char* file_extension,
    kernel_truth_diff_result* out_result) {
  if (out_result == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  if (
      (prev_size > 0 && prev_content == nullptr) ||
      (curr_size > 0 && curr_content == nullptr) || file_extension == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string_view prev(prev_content == nullptr ? "" : prev_content, prev_size);
  const std::string_view curr(curr_content == nullptr ? "" : curr_content, curr_size);
  return kernel::core::product::compute_truth_diff(prev, curr, file_extension, out_result);
}

extern "C" void kernel_free_truth_diff_result(kernel_truth_diff_result* result) {
  kernel::core::product::free_truth_diff_result(result);
}

extern "C" kernel_status kernel_get_truth_award_reason_key(
    const kernel_truth_award_reason reason,
    const char** out_key) {
  if (out_key == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  *out_key = nullptr;

  const char* key = kernel::core::product::truth_award_reason_key(reason);
  if (key == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  *out_key = key;
  return kernel::core::make_status(KERNEL_OK);
}
