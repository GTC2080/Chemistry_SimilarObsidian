// Reason: This file owns note upsert mutations and the shared derived-row replacement helper.

#include "storage/storage_note_lifecycle_internal.h"

#include "pdf/pdf_anchor.h"

#include <unordered_map>
#include <unordered_set>

namespace kernel::storage::detail {

namespace {

std::error_code load_existing_pdf_source_excerpt_snapshots(
    sqlite3* db,
    const sqlite3_int64 note_id,
    std::unordered_map<std::string, std::string>& out_snapshots) {
  out_snapshots.clear();

  sqlite3_stmt* stmt = nullptr;
  std::error_code ec = prepare(
      db,
      "SELECT anchor_serialized, excerpt_text "
      "FROM note_pdf_source_refs "
      "WHERE note_id=?1;",
      &stmt);
  if (ec) {
    return ec;
  }

  sqlite3_bind_int64(stmt, 1, note_id);
  return append_statement_rows(stmt, [&](sqlite3_stmt* row) {
    std::string anchor_serialized;
    std::string excerpt_text;
    assign_text_column(row, 0, anchor_serialized);
    assign_text_column(row, 1, excerpt_text);
    if (!anchor_serialized.empty() && !excerpt_text.empty()) {
      out_snapshots.insert_or_assign(std::move(anchor_serialized), std::move(excerpt_text));
    }
  });
}

std::error_code materialize_note_pdf_source_ref_row(
    sqlite3* db,
    const kernel::parser::PdfSourceRef& source_ref,
    const std::unordered_map<std::string, std::string>& prior_excerpt_snapshots,
    std::uint64_t& out_page,
    std::string& out_excerpt_text) {
  out_page = 0;
  out_excerpt_text.clear();

  const auto prior_snapshot = prior_excerpt_snapshots.find(source_ref.anchor_serialized);
  if (prior_snapshot != prior_excerpt_snapshots.end()) {
    out_excerpt_text = prior_snapshot->second;
  }

  kernel::pdf::ParsedPdfAnchor parsed_anchor;
  if (!kernel::pdf::parse_pdf_anchor(source_ref.anchor_serialized, parsed_anchor) ||
      parsed_anchor.rel_path != source_ref.pdf_rel_path) {
    return {};
  }

  out_page = parsed_anchor.page;
  Database storage_view{db};
  PdfMetadataRecord metadata;
  const std::error_code metadata_ec =
      read_live_pdf_metadata_record(storage_view, source_ref.pdf_rel_path, metadata);
  if (metadata_ec || metadata.is_missing) {
    return metadata_ec && metadata_ec != std::make_error_code(std::errc::no_such_file_or_directory)
               ? metadata_ec
               : std::error_code{};
  }

  PdfAnchorRecord current_anchor;
  const std::error_code anchor_ec =
      read_live_pdf_anchor_record(
          storage_view,
          source_ref.pdf_rel_path,
          parsed_anchor.page,
          current_anchor);
  const auto validation =
      kernel::pdf::validate_pdf_anchor(
          source_ref.anchor_serialized,
          &metadata,
          anchor_ec ? nullptr : &current_anchor);
  if (validation.state == kernel::pdf::PdfAnchorValidationState::Resolved &&
      out_excerpt_text.empty()) {
    out_excerpt_text = validation.current_anchor.excerpt_text;
  }

  return {};
}

}  // namespace

std::error_code replace_note_derived_rows(
    sqlite3* db,
    const sqlite3_int64 note_id,
    const kernel::parser::ParseResult& parse_result,
    std::string_view title,
    std::string_view body_text) {
  std::unordered_map<std::string, std::string> prior_pdf_excerpt_snapshots;
  std::error_code ec =
      load_existing_pdf_source_excerpt_snapshots(
          db,
          note_id,
          prior_pdf_excerpt_snapshots);
  if (ec) {
    return ec;
  }

  ec = clear_note_parse_rows(db, note_id);
  if (ec) {
    return ec;
  }

  ec = clear_note_attachment_rows(db, note_id);
  if (ec) {
    return ec;
  }

  ec = clear_note_pdf_source_ref_rows(db, note_id);
  if (ec) {
    return ec;
  }

  for (const auto& tag : parse_result.tags) {
    ec = insert_note_tag(db, note_id, tag);
    if (ec) {
      return ec;
    }
  }

  for (const auto& link : parse_result.wikilinks) {
    ec = insert_note_link(db, note_id, link);
    if (ec) {
      return ec;
    }
  }

  std::unordered_set<std::string> inserted_attachment_refs;
  for (const auto& attachment_ref : parse_result.attachment_refs) {
    if (!inserted_attachment_refs.insert(attachment_ref).second) {
      continue;
    }
    ec = insert_note_attachment_ref(db, note_id, attachment_ref);
    if (ec) {
      return ec;
    }
  }

  for (std::size_t index = 0; index < parse_result.pdf_source_refs.size(); ++index) {
    std::uint64_t page = 0;
    std::string excerpt_text;
    ec = materialize_note_pdf_source_ref_row(
        db,
        parse_result.pdf_source_refs[index],
        prior_pdf_excerpt_snapshots,
        page,
        excerpt_text);
    if (ec) {
      return ec;
    }

    ec = insert_note_pdf_source_ref(
        db,
        note_id,
        static_cast<std::int64_t>(index),
        parse_result.pdf_source_refs[index].pdf_rel_path,
        parse_result.pdf_source_refs[index].anchor_serialized,
        page,
        excerpt_text);
    if (ec) {
      return ec;
    }
  }

  return replace_note_fts_row(db, note_id, title, body_text);
}

}  // namespace kernel::storage::detail

namespace kernel::storage {

std::error_code upsert_note_metadata(
    Database& db,
    std::string_view rel_path,
    const kernel::platform::FileStat& stat,
    std::string_view content_revision,
    const kernel::parser::ParseResult& parse_result,
    std::string_view body_text) {
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  std::error_code ec = detail::begin_transaction(db.connection);
  if (ec) {
    return ec;
  }

  const std::string title =
      parse_result.title.empty() ? detail::title_from_rel_path(rel_path) : parse_result.title;

  sqlite3_stmt* stmt = nullptr;
  ec = detail::prepare(
      db.connection,
      "INSERT INTO notes(rel_path, title, file_size, mtime_ns, content_revision, is_deleted) "
      "VALUES(?1, ?2, ?3, ?4, ?5, 0) "
      "ON CONFLICT(rel_path) DO UPDATE SET "
      "  title=excluded.title,"
      "  file_size=excluded.file_size,"
      "  mtime_ns=excluded.mtime_ns,"
      "  content_revision=excluded.content_revision,"
      "  is_deleted=0;",
      &stmt);
  if (ec) {
    detail::rollback_transaction(db.connection);
    return ec;
  }

  sqlite3_bind_text(stmt, 1, std::string(rel_path).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, title.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(stat.file_size));
  sqlite3_bind_int64(stmt, 4, static_cast<sqlite3_int64>(stat.mtime_ns));
  sqlite3_bind_text(stmt, 5, std::string(content_revision).c_str(), -1, SQLITE_TRANSIENT);
  ec = detail::finalize_with_result(stmt, sqlite3_step(stmt));
  if (ec) {
    detail::rollback_transaction(db.connection);
    return ec;
  }

  sqlite3_int64 note_id = 0;
  ec = detail::lookup_note_id_by_rel_path(db.connection, rel_path, note_id);
  if (ec) {
    detail::rollback_transaction(db.connection);
    return ec;
  }

  ec = detail::replace_note_derived_rows(db.connection, note_id, parse_result, title, body_text);
  if (ec) {
    detail::rollback_transaction(db.connection);
    return ec;
  }

  ec = detail::commit_transaction(db.connection);
  if (ec) {
    detail::rollback_transaction(db.connection);
  }
  return ec;
}

}  // namespace kernel::storage
