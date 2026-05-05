// Reason: Keep semantic context and product path rules out of the public ABI wrapper.

#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace kernel::core::product {

std::size_t semantic_context_min_bytes();
std::string build_semantic_context(std::string_view content);
std::string derive_file_extension_from_path(std::string_view path);

}  // namespace kernel::core::product
