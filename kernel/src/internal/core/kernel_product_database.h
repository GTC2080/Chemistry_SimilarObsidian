// Reason: Keep product database normalization rules out of the public ABI
// wrapper so the wrapper only validates arguments and marshals buffers.

#pragma once

#include <string>
#include <string_view>

namespace kernel::core::product {

std::string normalize_database_column_type(std::string_view column_type);
bool normalize_database_json(std::string_view raw_json, std::string& out_json);

}  // namespace kernel::core::product
