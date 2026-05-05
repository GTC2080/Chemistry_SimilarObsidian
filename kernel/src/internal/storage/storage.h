// Reason: This file defines the smallest SQLite-backed storage API for schema v1 under state_dir.

#pragma once

#include "parser/parser.h"
#include "platform/platform.h"

#include <cstdint>
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

struct TagSummaryRecord {
  std::string name;
  std::uint32_t count = 0;
};

struct GraphNodeRecord {
  std::string id;
  std::string name;
  bool ghost = false;
};

struct GraphLinkRecord {
  std::string source;
  std::string target;
  std::string kind;
};

struct GraphRecord {
  std::vector<GraphNodeRecord> nodes;
  std::vector<GraphLinkRecord> links;
};

struct NoteCatalogRecord {
  std::string rel_path;
  std::string title;
  std::uint64_t file_size = 0;
  std::uint64_t mtime_ns = 0;
  std::string content_revision;
};

struct AiEmbeddingNoteMetadataRecord {
  std::string rel_path;
  std::string title;
  std::string absolute_path;
  std::int64_t created_at = 0;
  std::int64_t updated_at = 0;
};

struct AiEmbeddingTimestampRecord {
  std::string rel_path;
  std::int64_t updated_at = 0;
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

struct NotePdfSourceRefRecord {
  std::string pdf_rel_path;
  std::string anchor_serialized;
  std::string excerpt_text;
  std::uint64_t page = 0;
};

struct NoteChemSpectrumSourceRefRecord {
  std::string attachment_rel_path;
  std::string selector_serialized;
  std::string preview_text;
};

struct PdfSourceReferrerRecord {
  std::string note_rel_path;
  std::string note_title;
  std::string anchor_serialized;
  std::string excerpt_text;
  std::uint64_t page = 0;
};

struct ChemSpectrumReferrerRecord {
  std::string note_rel_path;
  std::string note_title;
  std::string selector_serialized;
  std::string preview_text;
};

struct PdfSourceRefDiagnosticsRecord {
  std::string note_rel_path;
  std::string pdf_rel_path;
  std::string anchor_serialized;
  std::string excerpt_text;
  std::uint64_t page = 0;
};

enum class PdfMetadataState : std::uint8_t {
  Unavailable = 0,
  Ready = 1,
  Partial = 2,
  Invalid = 3
};

enum class PdfDocTitleState : std::uint8_t {
  Unavailable = 0,
  Absent = 1,
  Available = 2
};

enum class PdfTextLayerState : std::uint8_t {
  Unavailable = 0,
  Absent = 1,
  Present = 2
};

struct PdfMetadataRecord {
  std::string rel_path;
  std::string attachment_content_revision;
  std::string pdf_metadata_revision;
  std::string doc_title;
  std::uint64_t page_count = 0;
  bool has_outline = false;
  bool is_missing = false;
  PdfMetadataState metadata_state = PdfMetadataState::Unavailable;
  PdfDocTitleState doc_title_state = PdfDocTitleState::Unavailable;
  PdfTextLayerState text_layer_state = PdfTextLayerState::Unavailable;
};

struct PdfAnchorRecord {
  std::string rel_path;
  std::string pdf_anchor_basis_revision;
  std::string anchor_serialized;
  std::string excerpt_text;
  std::string excerpt_fingerprint;
  std::uint64_t page = 0;
  bool is_missing = false;
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
std::error_code list_note_catalog_records(
    Database& db,
    std::size_t limit,
    std::vector<NoteCatalogRecord>& out_records);
std::error_code upsert_ai_embedding_note_metadata(
    Database& db,
    const AiEmbeddingNoteMetadataRecord& metadata);
std::error_code list_ai_embedding_note_timestamps(
    Database& db,
    std::vector<AiEmbeddingTimestampRecord>& out_records);
std::error_code update_ai_embedding(
    Database& db,
    std::string_view rel_path,
    const float* values,
    std::size_t value_count);
std::error_code clear_ai_embeddings(Database& db);
std::error_code delete_ai_embedding_note(
    Database& db,
    std::string_view rel_path,
    bool& out_deleted);
std::error_code list_top_ai_embedding_notes(
    Database& db,
    const float* query_values,
    std::size_t query_value_count,
    std::string_view exclude_rel_path,
    std::size_t limit,
    std::vector<NoteListHit>& out_hits);
std::error_code upsert_attachment_metadata(
    Database& db,
    std::string_view rel_path,
    const kernel::platform::FileStat& stat);
std::error_code mark_attachment_missing(Database& db, std::string_view rel_path);
std::error_code upsert_pdf_metadata(Database& db, const PdfMetadataRecord& record);
std::error_code replace_pdf_anchor_records(
    Database& db,
    std::string_view rel_path,
    const std::vector<PdfAnchorRecord>& records);
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
std::error_code read_live_pdf_metadata_record(
    Database& db,
    std::string_view rel_path,
    PdfMetadataRecord& out_record);
std::error_code list_live_pdf_anchor_records(
    Database& db,
    std::string_view rel_path,
    std::vector<PdfAnchorRecord>& out_records);
std::error_code read_live_pdf_anchor_record(
    Database& db,
    std::string_view rel_path,
    std::uint64_t page,
    PdfAnchorRecord& out_record);
std::error_code list_note_pdf_source_ref_records(
    Database& db,
    std::string_view note_rel_path,
    std::size_t limit,
    std::vector<NotePdfSourceRefRecord>& out_records);
std::error_code list_note_chem_spectrum_source_ref_records(
    Database& db,
    std::string_view note_rel_path,
    std::size_t limit,
    std::vector<NoteChemSpectrumSourceRefRecord>& out_records);
std::error_code list_pdf_source_referrer_records(
    Database& db,
    std::string_view pdf_rel_path,
    std::size_t limit,
    std::vector<PdfSourceReferrerRecord>& out_records);
std::error_code list_chem_spectrum_referrer_records(
    Database& db,
    std::string_view attachment_rel_path,
    std::size_t limit,
    std::vector<ChemSpectrumReferrerRecord>& out_records);
std::error_code count_attachments(Database& db, std::uint64_t& out_count);
std::error_code count_missing_attachments(Database& db, std::uint64_t& out_count);
std::error_code count_orphaned_attachments(Database& db, std::uint64_t& out_count);
std::error_code count_live_pdf_records(Database& db, std::uint64_t& out_count);
std::error_code count_live_pdf_records_by_state(
    Database& db,
    PdfMetadataState state,
    std::uint64_t& out_count);
std::error_code count_live_pdf_anchor_records(Database& db, std::uint64_t& out_count);
std::error_code list_live_pdf_source_ref_diagnostics_records(
    Database& db,
    std::vector<PdfSourceRefDiagnosticsRecord>& out_records);
std::error_code list_live_chem_spectrum_source_ref_diagnostics_records(
    Database& db,
    std::vector<NoteChemSpectrumSourceRefRecord>& out_records);
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
std::error_code list_tag_summaries(
    Database& db,
    std::size_t limit,
    std::vector<TagSummaryRecord>& out_records);
std::error_code build_note_graph(
    Database& db,
    std::size_t note_limit,
    GraphRecord& out_graph);
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
