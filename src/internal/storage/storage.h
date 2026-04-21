// Reason: This file defines the smallest SQLite-backed storage API for schema v1 under state_dir.

#pragma once

#include "parser/parser.h"
#include "platform/platform.h"

#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

struct sqlite3;

namespace kernel::storage {

struct Database {
  sqlite3* connection = nullptr;
};

struct NoteListHit {
  std::string rel_path;
  std::string title;
};

struct AttachmentMetadataRecord {
  std::uint64_t file_size = 0;
  std::uint64_t mtime_ns = 0;
  bool is_missing = false;
};

struct AttachmentCatalogRecord {
  std::string rel_path;
  std::uint64_t file_size = 0;
  std::uint64_t mtime_ns = 0;
  std::uint64_t ref_count = 0;
  bool is_missing = false;
};

struct AttachmentReferrerRecord {
  std::string note_rel_path;
  std::string note_title;
};

std::error_code open_or_create(const std::filesystem::path& db_path, Database& out_db);
void close(Database& db);
std::error_code ensure_schema_v1(Database& db);
std::error_code upsert_note_metadata(
    Database& db,
    std::string_view rel_path,
    const kernel::platform::FileStat& stat,
    std::string_view content_revision,
    const kernel::parser::ParseResult& parse_result,
    std::string_view body_text);
std::error_code mark_note_deleted(Database& db, std::string_view rel_path);
std::error_code rename_note_metadata(
    Database& db,
    std::string_view old_rel_path,
    std::string_view new_rel_path,
    const kernel::platform::FileStat& stat,
    std::string_view content_revision,
    const kernel::parser::ParseResult& parse_result,
    std::string_view body_text);
std::error_code list_note_paths(Database& db, std::vector<std::string>& out_paths);
std::error_code upsert_attachment_metadata(
    Database& db,
    std::string_view rel_path,
    const kernel::platform::FileStat& stat);
std::error_code mark_attachment_missing(Database& db, std::string_view rel_path);
std::error_code list_attachment_paths(Database& db, std::vector<std::string>& out_paths);
std::error_code list_note_attachment_refs(
    Database& db,
    std::string_view note_rel_path,
    std::vector<std::string>& out_refs);
std::error_code list_live_attachment_records(
    Database& db,
    std::size_t limit,
    std::vector<AttachmentCatalogRecord>& out_records);
std::error_code read_live_attachment_record(
    Database& db,
    std::string_view rel_path,
    AttachmentCatalogRecord& out_record);
std::error_code list_note_attachment_records(
    Database& db,
    std::string_view note_rel_path,
    std::size_t limit,
    std::vector<AttachmentCatalogRecord>& out_records);
std::error_code list_attachment_referrers(
    Database& db,
    std::string_view attachment_rel_path,
    std::size_t limit,
    std::vector<AttachmentReferrerRecord>& out_referrers);
std::error_code read_attachment_metadata(
    Database& db,
    std::string_view rel_path,
    AttachmentMetadataRecord& out_metadata);
std::error_code count_attachments(Database& db, std::uint64_t& out_count);
std::error_code count_missing_attachments(Database& db, std::uint64_t& out_count);
std::error_code count_orphaned_attachments(Database& db, std::uint64_t& out_count);
std::error_code list_missing_attachment_paths(
    Database& db,
    std::size_t limit,
    std::vector<std::string>& out_paths);
std::error_code list_orphaned_attachment_paths(
    Database& db,
    std::size_t limit,
    std::vector<std::string>& out_paths);
std::error_code count_active_notes(Database& db, std::uint64_t& out_count);
std::error_code list_notes_by_tag(
    Database& db,
    std::string_view tag,
    std::size_t limit,
    std::vector<NoteListHit>& out_hits);
std::error_code list_backlinks_for_rel_path(
    Database& db,
    std::string_view rel_path,
    std::size_t limit,
    std::vector<NoteListHit>& out_hits);
std::error_code insert_journal_state(
    Database& db,
    std::string_view op_id,
    std::string_view op_type,
    std::string_view rel_path,
    std::string_view temp_path,
    std::string_view phase);

}  // namespace kernel::storage
