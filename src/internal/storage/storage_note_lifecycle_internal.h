// Reason: This file keeps the small lifecycle-only helper shared by split note mutation units.

#pragma once

#include "storage/storage_internal.h"

namespace kernel::storage::detail {

std::error_code replace_note_derived_rows(
    sqlite3* db,
    sqlite3_int64 note_id,
    const kernel::parser::ParseResult& parse_result,
    std::string_view title,
    std::string_view body_text);

}  // namespace kernel::storage::detail
