// Reason: This file defines the narrow Windows-first file operations the core needs without leaking OS details upward.

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

namespace kernel::platform {

struct FileStat {
  bool exists = false;
  bool is_regular_file = false;
  std::uint64_t file_size = 0;
  std::uint64_t mtime_ns = 0;
};

struct ReadFileResult {
  std::string bytes;
  FileStat stat;
};

std::error_code directory_exists(const std::filesystem::path& path, bool& out_exists);
std::error_code local_app_data_directory(std::filesystem::path& out_path);
std::error_code ensure_directory(const std::filesystem::path& path);
std::error_code stat_file(const std::filesystem::path& path, FileStat& out_stat);
std::error_code read_file(const std::filesystem::path& path, ReadFileResult& out_result);
std::error_code ensure_parent_directory(const std::filesystem::path& path);
std::error_code write_temp_file(
    const std::filesystem::path& final_path,
    std::string_view bytes,
    std::filesystem::path& out_temp_path);
std::error_code atomic_replace_file(
    const std::filesystem::path& temp_path,
    const std::filesystem::path& final_path);
std::error_code remove_file_if_exists(const std::filesystem::path& path);
std::error_code append_file_frame(const std::filesystem::path& path, std::string_view bytes);

}  // namespace kernel::platform
