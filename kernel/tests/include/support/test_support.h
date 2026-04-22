// Reason: This file centralizes generic test helpers so large test sources can focus on behavior instead of plumbing.

#pragma once

#include "kernel/c_api.h"

#include "third_party/sqlite/sqlite3.h"

#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

std::filesystem::path make_temp_vault(std::string_view prefix = "chem_kernel_test_");

void expect_ok(kernel_status status);
void require_true(bool condition, std::string_view message);

std::filesystem::path state_dir_for_vault(const std::filesystem::path& vault);
std::filesystem::path storage_db_for_vault(const std::filesystem::path& vault);
std::filesystem::path journal_path_for_vault(const std::filesystem::path& vault);

void write_file_bytes(const std::filesystem::path& path, std::string_view bytes);
std::string read_file_text(const std::filesystem::path& path);
void append_raw_bytes(const std::filesystem::path& path, std::string_view bytes);

sqlite3* open_sqlite_readonly(const std::filesystem::path& db_path);
sqlite3* open_sqlite_readwrite(const std::filesystem::path& db_path);
int query_single_int(sqlite3* db, std::string_view sql);
std::string query_single_text(sqlite3* db, std::string_view sql);
void exec_sql(sqlite3* db, std::string_view sql);

template <typename Predicate>
void require_eventually(Predicate&& predicate, std::string_view message) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (std::chrono::steady_clock::now() < deadline) {
    if (predicate()) {
      return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
  }
  throw std::runtime_error("requirement failed: " + std::string(message));
}
