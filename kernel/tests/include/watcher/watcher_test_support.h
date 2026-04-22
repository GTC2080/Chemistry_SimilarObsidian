#pragma once

#include "storage/storage.h"

#include <filesystem>

namespace watcher_tests {

std::filesystem::path make_temp_watch_root();

kernel::storage::Database open_search_db(const std::filesystem::path& vault);

}  // namespace watcher_tests
