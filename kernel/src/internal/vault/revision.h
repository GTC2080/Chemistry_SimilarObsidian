// Reason: This file isolates the frozen content revision rule into a small testable unit.

#pragma once

#include <string>
#include <string_view>

namespace kernel::vault {

std::string sha256_hex(std::string_view bytes);
std::string compute_content_revision(std::string_view bytes);

}  // namespace kernel::vault
