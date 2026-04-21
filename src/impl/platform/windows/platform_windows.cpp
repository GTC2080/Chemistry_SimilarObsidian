// Reason: This file implements the Windows-first filesystem primitives behind the narrow platform boundary.

#include "platform/platform.h"

#include <Windows.h>

#include <chrono>
#include <fstream>
#include <sstream>

namespace kernel::platform {
namespace {

std::uint64_t to_mtime_ns(const std::filesystem::file_time_type& value) {
  const auto duration = value.time_since_epoch();
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count());
}

std::wstring to_wide(const std::filesystem::path& path) {
  return path.wstring();
}

std::filesystem::path make_temp_path(const std::filesystem::path& final_path) {
  std::wstringstream stream;
  stream << final_path.native() << L".tmp." << GetCurrentProcessId() << L"." << GetTickCount64();
  return std::filesystem::path(stream.str());
}

}  // namespace

std::error_code directory_exists(const std::filesystem::path& path, bool& out_exists) {
  std::error_code ec;
  out_exists = std::filesystem::is_directory(path, ec);
  return ec;
}

std::error_code local_app_data_directory(std::filesystem::path& out_path) {
  out_path.clear();

  const DWORD required = GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0);
  if (required == 0) {
    return std::error_code(static_cast<int>(GetLastError()), std::system_category());
  }

  std::wstring buffer(required, L'\0');
  const DWORD written = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer.data(), required);
  if (written == 0 || written >= required) {
    return std::error_code(static_cast<int>(GetLastError()), std::system_category());
  }

  buffer.resize(written);
  out_path = std::filesystem::path(buffer);
  return {};
}

std::error_code ensure_directory(const std::filesystem::path& path) {
  std::error_code ec;
  std::filesystem::create_directories(path, ec);
  return ec;
}

std::error_code stat_file(const std::filesystem::path& path, FileStat& out_stat) {
  out_stat = {};

  std::error_code ec;
  const auto status = std::filesystem::status(path, ec);
  if (ec) {
    if (ec == std::errc::no_such_file_or_directory) {
      return {};
    }
    return ec;
  }

  out_stat.exists = std::filesystem::exists(status);
  out_stat.is_regular_file = std::filesystem::is_regular_file(status);
  if (!out_stat.exists || !out_stat.is_regular_file) {
    return {};
  }

  out_stat.file_size = static_cast<std::uint64_t>(std::filesystem::file_size(path, ec));
  if (ec) {
    return ec;
  }

  out_stat.mtime_ns = to_mtime_ns(std::filesystem::last_write_time(path, ec));
  return ec;
}

std::error_code read_file(const std::filesystem::path& path, ReadFileResult& out_result) {
  out_result = {};

  FileStat stat{};
  std::error_code ec = stat_file(path, stat);
  if (ec) {
    return ec;
  }
  if (!stat.exists || !stat.is_regular_file) {
    return std::make_error_code(std::errc::no_such_file_or_directory);
  }

  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return std::make_error_code(std::errc::io_error);
  }

  out_result.bytes.assign(
      std::istreambuf_iterator<char>(input),
      std::istreambuf_iterator<char>());

  if (!input.good() && !input.eof()) {
    return std::make_error_code(std::errc::io_error);
  }

  out_result.stat = stat;
  return {};
}

std::error_code ensure_parent_directory(const std::filesystem::path& path) {
  std::error_code ec;
  const auto parent = path.parent_path();
  if (parent.empty()) {
    return {};
  }
  std::filesystem::create_directories(parent, ec);
  return ec;
}

std::error_code write_temp_file(
    const std::filesystem::path& final_path,
    std::string_view bytes,
    std::filesystem::path& out_temp_path) {
  out_temp_path.clear();

  std::error_code ec = ensure_parent_directory(final_path);
  if (ec) {
    return ec;
  }

  const auto temp_path = make_temp_path(final_path);
  {
    std::ofstream output(temp_path, std::ios::binary | std::ios::trunc);
    if (!output) {
      return std::make_error_code(std::errc::io_error);
    }

    if (!bytes.empty()) {
      output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
    output.flush();

    if (!output) {
      std::filesystem::remove(temp_path, ec);
      return std::make_error_code(std::errc::io_error);
    }
  }

  out_temp_path = temp_path;
  return {};
}

std::error_code atomic_replace_file(
    const std::filesystem::path& temp_path,
    const std::filesystem::path& final_path) {
  const auto from = to_wide(temp_path);
  const auto to = to_wide(final_path);
  const BOOL moved = MoveFileExW(
      from.c_str(),
      to.c_str(),
      MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
  if (!moved) {
    return std::error_code(static_cast<int>(GetLastError()), std::system_category());
  }
  return {};
}

std::error_code remove_file_if_exists(const std::filesystem::path& path) {
  std::error_code ec;
  std::filesystem::remove(path, ec);
  if (ec == std::errc::no_such_file_or_directory) {
    return {};
  }
  return ec;
}

std::error_code append_file_frame(const std::filesystem::path& path, std::string_view bytes) {
  std::error_code ec = ensure_parent_directory(path);
  if (ec) {
    return ec;
  }

  std::ofstream output(path, std::ios::binary | std::ios::app);
  if (!output) {
    return std::make_error_code(std::errc::io_error);
  }

  if (!bytes.empty()) {
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  }
  output.flush();
  if (!output) {
    return std::make_error_code(std::errc::io_error);
  }
  return {};
}

}  // namespace kernel::platform
