#pragma once

#include "storage/storage.h"

#include <filesystem>

namespace search_tests {

kernel::storage::Database open_search_db(const std::filesystem::path& vault);

}  // namespace search_tests
