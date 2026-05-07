// Reason: This file provides generic filesystem, sqlite, and polling helpers shared by multiple large test suites.

#include "support/test_support.h"

#include <fstream>
#include <stdexcept>

std::filesystem::path make_temp_vault(std::string_view prefix) {
  const auto now = std::filesystem::file_time_type::clock::now().time_since_epoch().count();
  const auto path =
      std::filesystem::temp_directory_path() / (std::string(prefix) + std::to_string(now));
  std::filesystem::create_directories(path);
  return path;
}

void expect_ok(const kernel_status status) {
  if (status.code != KERNEL_OK) {
    throw std::runtime_error("expected KERNEL_OK but got " + std::to_string(status.code));
  }
}

void require_true(const bool condition, std::string_view message) {
  if (!condition) {
    throw std::runtime_error("requirement failed: " + std::string(message));
  }
}

std::filesystem::path state_dir_for_vault(const std::filesystem::path& vault) {
  return vault.lexically_normal() / ".nexus" / "kernel";
}

std::filesystem::path storage_db_for_vault(const std::filesystem::path& vault) {
  return state_dir_for_vault(vault) / "state.sqlite3";
}

std::filesystem::path journal_path_for_vault(const std::filesystem::path& vault) {
  return state_dir_for_vault(vault) / "recovery.journal";
}

void write_file_bytes(const std::filesystem::path& path, std::string_view bytes) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  require_true(output.good(), "seed file must be writable");
  output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  output.flush();
  require_true(output.good(), "seed file write must succeed");
}

std::string read_file_text(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  require_true(input.good(), "text file must be readable");
  return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

void append_raw_bytes(const std::filesystem::path& path, std::string_view bytes) {
  std::ofstream output(path, std::ios::binary | std::ios::app);
  require_true(output.good(), "raw append target must be writable");
  output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  output.flush();
  require_true(output.good(), "raw append must succeed");
}

sqlite3* open_sqlite_readonly(const std::filesystem::path& db_path) {
  sqlite3* db = nullptr;
  const int rc =
      sqlite3_open_v2(db_path.string().c_str(), &db, SQLITE_OPEN_READONLY, nullptr);
  require_true(rc == SQLITE_OK, "sqlite database should open");
  return db;
}

sqlite3* open_sqlite_readwrite(const std::filesystem::path& db_path) {
  sqlite3* db = nullptr;
  const int rc =
      sqlite3_open_v2(db_path.string().c_str(), &db, SQLITE_OPEN_READWRITE, nullptr);
  require_true(rc == SQLITE_OK, "sqlite database should open readwrite");
  return db;
}

int query_single_int(sqlite3* db, std::string_view sql) {
  sqlite3_stmt* stmt = nullptr;
  const int prepare_rc = sqlite3_prepare_v2(db, std::string(sql).c_str(), -1, &stmt, nullptr);
  require_true(prepare_rc == SQLITE_OK, "sqlite statement should prepare");
  const int step_rc = sqlite3_step(stmt);
  require_true(step_rc == SQLITE_ROW, std::string("sqlite query should yield one row: ") + std::string(sql));
  const int value = sqlite3_column_int(stmt, 0);
  sqlite3_finalize(stmt);
  return value;
}

std::string query_single_text(sqlite3* db, std::string_view sql) {
  sqlite3_stmt* stmt = nullptr;
  const int prepare_rc = sqlite3_prepare_v2(db, std::string(sql).c_str(), -1, &stmt, nullptr);
  require_true(prepare_rc == SQLITE_OK, "sqlite statement should prepare");
  const int step_rc = sqlite3_step(stmt);
  require_true(step_rc == SQLITE_ROW, std::string("sqlite query should yield one row: ") + std::string(sql));
  const unsigned char* value = sqlite3_column_text(stmt, 0);
  require_true(value != nullptr, "sqlite text column should not be null");
  const std::string result(reinterpret_cast<const char*>(value));
  sqlite3_finalize(stmt);
  return result;
}

void exec_sql(sqlite3* db, std::string_view sql) {
  char* error_message = nullptr;
  const int rc = sqlite3_exec(db, std::string(sql).c_str(), nullptr, nullptr, &error_message);
  if (error_message != nullptr) {
    const std::string message(error_message);
    sqlite3_free(error_message);
    throw std::runtime_error("sqlite exec failed: " + message);
  }
  require_true(rc == SQLITE_OK, "sqlite exec should succeed");
}
