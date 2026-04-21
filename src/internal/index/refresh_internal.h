// Reason: This file keeps the small private helpers shared by the split refresh implementation units.

#pragma once

#include <filesystem>
#include <stop_token>

namespace kernel::index {

bool is_markdown_rel_path(const std::filesystem::path& path);
bool stop_requested(std::stop_token stop_token);

}  // namespace kernel::index
