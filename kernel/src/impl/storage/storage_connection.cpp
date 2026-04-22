// Reason: This file owns opening and closing the SQLite storage database.

#include "storage/storage_internal.h"

namespace kernel::storage {

std::error_code open_or_create(const std::filesystem::path& db_path, Database& out_db) {
  close(out_db);

  std::error_code ec = kernel::platform::ensure_parent_directory(db_path);
  if (ec) {
    return ec;
  }

  sqlite3* db = nullptr;
  const int rc = sqlite3_open_v2(
      db_path.string().c_str(),
      &db,
      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
      nullptr);
  if (rc != SQLITE_OK) {
    if (db != nullptr) {
      sqlite3_close(db);
    }
    return detail::sqlite_error(db, rc);
  }

  out_db.connection = db;
  return {};
}

void close(Database& db) {
  if (db.connection != nullptr) {
    sqlite3_close(db.connection);
    db.connection = nullptr;
  }
}

}  // namespace kernel::storage
