// Reason: This file owns attachment-oriented read queries, live-catalog reads,
// and attachment support-bundle counts.

#include "storage/storage_internal.h"

namespace kernel::storage {

namespace {

sqlite3_int64 normalize_limit(const std::size_t limit) {
  return limit == static_cast<std::size_t>(-1)
             ? static_cast<sqlite3_int64>(-1)
             : static_cast<sqlite3_int64>(limit);
}

constexpr char kLiveAttachmentCatalogSelect[] =
    "SELECT refs.attachment_rel_path, "
    "       COALESCE(attachments.file_size, 0), "
    "       COALESCE(attachments.mtime_ns, 0), "
    "       COALESCE(attachments.is_missing, 1), "
    "       COUNT(*) "
    "FROM note_attachment_refs AS refs "
    "JOIN notes ON notes.note_id = refs.note_id AND notes.is_deleted = 0 "
    "LEFT JOIN attachments ON attachments.rel_path = refs.attachment_rel_path ";

constexpr char kLiveAttachmentCountSql[] =
    "SELECT COUNT(*) "
    "FROM ("
    "  SELECT DISTINCT note_attachment_refs.attachment_rel_path "
    "  FROM note_attachment_refs "
    "  JOIN notes ON notes.note_id = note_attachment_refs.note_id "
    "  WHERE notes.is_deleted = 0"
    ");";

constexpr char kMissingLiveAttachmentCountSql[] =
    "SELECT COUNT(*) "
    "FROM ("
    "  SELECT DISTINCT note_attachment_refs.attachment_rel_path "
    "  FROM note_attachment_refs "
    "  JOIN notes ON notes.note_id = note_attachment_refs.note_id "
    "  JOIN attachments ON attachments.rel_path = note_attachment_refs.attachment_rel_path "
    "  WHERE notes.is_deleted = 0 AND attachments.is_missing = 1"
    ");";

std::error_code lookup_active_note_id_by_rel_path(
    sqlite3* db,
    std::string_view note_rel_path,
    sqlite3_int64& out_note_id) {
  sqlite3_stmt* stmt = nullptr;
  std::error_code ec = detail::prepare(
      db,
      "SELECT note_id FROM notes WHERE rel_path=?1 AND is_deleted=0;",
      &stmt);
  if (ec) {
    return ec;
  }

  sqlite3_bind_text(stmt, 1, std::string(note_rel_path).c_str(), -1, SQLITE_TRANSIENT);
  const int step_rc = sqlite3_step(stmt);
  if (step_rc == SQLITE_DONE) {
    sqlite3_finalize(stmt);
    return std::make_error_code(std::errc::no_such_file_or_directory);
  }
  if (step_rc != SQLITE_ROW) {
    sqlite3_finalize(stmt);
    return std::error_code(step_rc, std::generic_category());
  }

  out_note_id = sqlite3_column_int64(stmt, 0);
  return detail::finalize_with_result(stmt, step_rc);
}

AttachmentCatalogRecord read_attachment_catalog_record(sqlite3_stmt* stmt) {
  AttachmentCatalogRecord record{};
  detail::assign_text_column(stmt, 0, record.rel_path);
  record.file_size = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 1));
  record.mtime_ns = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 2));
  record.is_missing = sqlite3_column_int(stmt, 3) != 0;
  record.ref_count = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 4));
  return record;
}

AttachmentReferrerRecord read_attachment_referrer_record(sqlite3_stmt* stmt) {
  AttachmentReferrerRecord record{};
  detail::assign_text_column(stmt, 0, record.note_rel_path);
  detail::assign_text_column(stmt, 1, record.note_title);
  return record;
}

}  // namespace

std::error_code list_attachment_paths(Database& db, std::vector<std::string>& out_paths) {
  out_paths.clear();
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  sqlite3_stmt* stmt = nullptr;
  std::error_code ec = detail::prepare(
      db.connection,
      "SELECT rel_path FROM attachments ORDER BY rel_path ASC;",
      &stmt);
  if (ec) {
    return ec;
  }

  return detail::append_statement_rows(stmt, [&](sqlite3_stmt* row) {
    std::string path;
    detail::assign_text_column(row, 0, path);
    out_paths.push_back(std::move(path));
  });
}

std::error_code list_note_attachment_refs(
    Database& db,
    std::string_view note_rel_path,
    std::vector<std::string>& out_refs) {
  out_refs.clear();
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  sqlite3_int64 note_id = 0;
  std::error_code ec = lookup_active_note_id_by_rel_path(db.connection, note_rel_path, note_id);
  if (ec) {
    return ec;
  }

  sqlite3_stmt* stmt = nullptr;
  ec = detail::prepare(
      db.connection,
      "SELECT attachment_rel_path "
      "FROM note_attachment_refs "
      "WHERE note_id=?1 "
      "ORDER BY rowid ASC;",
      &stmt);
  if (ec) {
    return ec;
  }

  sqlite3_bind_int64(stmt, 1, note_id);
  return detail::append_statement_rows(stmt, [&](sqlite3_stmt* row) {
    std::string ref;
    detail::assign_text_column(row, 0, ref);
    out_refs.push_back(std::move(ref));
  });
}

std::error_code list_live_attachment_records(
    Database& db,
    const std::size_t limit,
    std::vector<AttachmentCatalogRecord>& out_records) {
  out_records.clear();
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  sqlite3_stmt* stmt = nullptr;
  const std::string sql = std::string(kLiveAttachmentCatalogSelect) +
                          "GROUP BY refs.attachment_rel_path "
                          "ORDER BY refs.attachment_rel_path ASC "
                          "LIMIT ?1;";
  std::error_code ec = detail::prepare(db.connection, sql.c_str(), &stmt);
  if (ec) {
    return ec;
  }

  sqlite3_bind_int64(stmt, 1, normalize_limit(limit));
  return detail::append_statement_rows(stmt, [&](sqlite3_stmt* row) {
    out_records.push_back(read_attachment_catalog_record(row));
  });
}

std::error_code read_live_attachment_record(
    Database& db,
    std::string_view rel_path,
    AttachmentCatalogRecord& out_record) {
  out_record = AttachmentCatalogRecord{};
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  sqlite3_stmt* stmt = nullptr;
  const std::string sql = std::string(kLiveAttachmentCatalogSelect) +
                          "WHERE refs.attachment_rel_path = ?1 "
                          "GROUP BY refs.attachment_rel_path;";
  std::error_code ec = detail::prepare(db.connection, sql.c_str(), &stmt);
  if (ec) {
    return ec;
  }

  sqlite3_bind_text(stmt, 1, std::string(rel_path).c_str(), -1, SQLITE_TRANSIENT);
  const int step_rc = sqlite3_step(stmt);
  if (step_rc == SQLITE_DONE) {
    sqlite3_finalize(stmt);
    return std::make_error_code(std::errc::no_such_file_or_directory);
  }
  if (step_rc != SQLITE_ROW) {
    sqlite3_finalize(stmt);
    return std::error_code(step_rc, std::generic_category());
  }

  out_record = read_attachment_catalog_record(stmt);
  return detail::finalize_with_result(stmt, step_rc);
}

std::error_code list_note_attachment_records(
    Database& db,
    std::string_view note_rel_path,
    const std::size_t limit,
    std::vector<AttachmentCatalogRecord>& out_records) {
  out_records.clear();
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  sqlite3_int64 note_id = 0;
  std::error_code ec = lookup_active_note_id_by_rel_path(db.connection, note_rel_path, note_id);
  if (ec) {
    return ec;
  }

  sqlite3_stmt* stmt = nullptr;
  ec = detail::prepare(
      db.connection,
      "SELECT refs.attachment_rel_path, "
      "       COALESCE(attachments.file_size, 0), "
      "       COALESCE(attachments.mtime_ns, 0), "
      "       COALESCE(attachments.is_missing, 1), "
      "       ("
      "         SELECT COUNT(*) "
      "         FROM note_attachment_refs AS live_refs "
      "         JOIN notes AS live_notes "
      "           ON live_notes.note_id = live_refs.note_id "
      "          AND live_notes.is_deleted = 0 "
      "         WHERE live_refs.attachment_rel_path = refs.attachment_rel_path"
      "       ) "
      "FROM note_attachment_refs AS refs "
      "LEFT JOIN attachments ON attachments.rel_path = refs.attachment_rel_path "
      "WHERE refs.note_id = ?1 "
      "ORDER BY refs.rowid ASC "
      "LIMIT ?2;",
      &stmt);
  if (ec) {
    return ec;
  }

  sqlite3_bind_int64(stmt, 1, note_id);
  sqlite3_bind_int64(stmt, 2, normalize_limit(limit));
  return detail::append_statement_rows(stmt, [&](sqlite3_stmt* row) {
    out_records.push_back(read_attachment_catalog_record(row));
  });
}

std::error_code list_attachment_referrers(
    Database& db,
    std::string_view attachment_rel_path,
    const std::size_t limit,
    std::vector<AttachmentReferrerRecord>& out_referrers) {
  out_referrers.clear();
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  sqlite3_stmt* stmt = nullptr;
  std::error_code ec = detail::prepare(
      db.connection,
      "SELECT notes.rel_path, notes.title "
      "FROM note_attachment_refs AS refs "
      "JOIN notes ON notes.note_id = refs.note_id AND notes.is_deleted = 0 "
      "WHERE refs.attachment_rel_path = ?1 "
      "ORDER BY notes.rel_path ASC "
      "LIMIT ?2;",
      &stmt);
  if (ec) {
    return ec;
  }

  sqlite3_bind_text(stmt, 1, std::string(attachment_rel_path).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 2, normalize_limit(limit));
  bool saw_row = false;
  const std::error_code append_ec = detail::append_statement_rows(stmt, [&](sqlite3_stmt* row) {
    saw_row = true;
    out_referrers.push_back(read_attachment_referrer_record(row));
  });
  if (!append_ec && !saw_row) {
    return std::make_error_code(std::errc::no_such_file_or_directory);
  }
  return append_ec;
}

std::error_code read_attachment_metadata(
    Database& db,
    std::string_view rel_path,
    AttachmentMetadataRecord& out_metadata) {
  out_metadata = AttachmentMetadataRecord{};
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  sqlite3_stmt* stmt = nullptr;
  std::error_code ec = detail::prepare(
      db.connection,
      "SELECT file_size, mtime_ns, is_missing "
      "FROM attachments "
      "WHERE rel_path=?1;",
      &stmt);
  if (ec) {
    return ec;
  }

  sqlite3_bind_text(stmt, 1, std::string(rel_path).c_str(), -1, SQLITE_TRANSIENT);
  const int step_rc = sqlite3_step(stmt);
  if (step_rc == SQLITE_DONE) {
    sqlite3_finalize(stmt);
    return std::make_error_code(std::errc::no_such_file_or_directory);
  }
  if (step_rc != SQLITE_ROW) {
    sqlite3_finalize(stmt);
    return std::error_code(step_rc, std::generic_category());
  }

  out_metadata.file_size = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 0));
  out_metadata.mtime_ns = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 1));
  out_metadata.is_missing = sqlite3_column_int(stmt, 2) != 0;
  return detail::finalize_with_result(stmt, step_rc);
}

std::error_code count_attachments(Database& db, std::uint64_t& out_count) {
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  return detail::scalar_count_query(db.connection, kLiveAttachmentCountSql, out_count);
}

std::error_code count_missing_attachments(Database& db, std::uint64_t& out_count) {
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  return detail::scalar_count_query(db.connection, kMissingLiveAttachmentCountSql, out_count);
}

}  // namespace kernel::storage
