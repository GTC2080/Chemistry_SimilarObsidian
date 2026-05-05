// Reason: Keep truth award diff rules out of the product public ABI wrapper.

#pragma once

#include "kernel/c_api.h"

#include <string>
#include <string_view>

namespace kernel::core::product {

kernel_status compute_truth_diff(
    std::string_view prev_content,
    std::string_view curr_content,
    std::string_view file_extension,
    kernel_truth_diff_result* out_result);

void free_truth_diff_result(kernel_truth_diff_result* result);
const char* truth_award_reason_key(kernel_truth_award_reason reason);
std::string route_truth_attribute_by_extension(std::string_view ext);

}  // namespace kernel::core::product
