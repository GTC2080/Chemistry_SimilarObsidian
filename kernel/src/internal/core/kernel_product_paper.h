// Reason: Keep paper compile planning and log summarization rules out of the
// product public ABI wrapper.

#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace kernel::core::product {

std::string_view default_paper_template();

std::string build_paper_compile_plan_json(
    std::string_view workspace,
    std::string_view template_name,
    const char* const* image_paths,
    const std::size_t* image_path_sizes,
    std::size_t image_path_count,
    std::string_view csl_path,
    std::string_view bibliography_path,
    std::string_view resource_separator);

std::string build_paper_compile_log_summary_json(
    std::string_view log,
    std::size_t log_char_limit);

}  // namespace kernel::core::product
