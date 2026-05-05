// Reason: This file stores AI embedding metadata and vectors in the kernel-owned state database.

#include "storage/storage_internal.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace kernel::storage {

namespace {

struct ScoredNote {
  NoteListHit hit;
  float score = 0.0f;
};

void append_f32_le(std::string& blob, const float value) {
  std::uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  blob.push_back(static_cast<char>(bits & 0xffu));
  blob.push_back(static_cast<char>((bits >> 8) & 0xffu));
  blob.push_back(static_cast<char>((bits >> 16) & 0xffu));
  blob.push_back(static_cast<char>((bits >> 24) & 0xffu));
}

std::string serialize_embedding_blob(const float* values, const std::size_t value_count) {
  std::string blob;
  blob.reserve(value_count * sizeof(float));
  for (std::size_t index = 0; index < value_count; ++index) {
    append_f32_le(blob, values[index]);
  }
  return blob;
}

bool parse_embedding_blob(const void* data, const int byte_count, std::vector<float>& out_values) {
  out_values.clear();
  if (data == nullptr || byte_count <= 0 || byte_count % 4 != 0) {
    return false;
  }

  const auto* bytes = static_cast<const unsigned char*>(data);
  const std::size_t value_count = static_cast<std::size_t>(byte_count) / 4;
  out_values.reserve(value_count);
  for (std::size_t index = 0; index < value_count; ++index) {
    const std::size_t offset = index * 4;
    const std::uint32_t bits =
        static_cast<std::uint32_t>(bytes[offset]) |
        (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
        (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
        (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    out_values.push_back(value);
  }
  return true;
}

float cosine_similarity(
    const float* query_values,
    const std::size_t query_value_count,
    const std::vector<float>& embedding) {
  const std::size_t count = std::min(query_value_count, embedding.size());
  if (count == 0) {
    return 0.0f;
  }

  float dot = 0.0f;
  float query_norm = 0.0f;
  float embedding_norm = 0.0f;
  for (std::size_t index = 0; index < count; ++index) {
    dot += query_values[index] * embedding[index];
    query_norm += query_values[index] * query_values[index];
    embedding_norm += embedding[index] * embedding[index];
  }

  const float denominator = std::sqrt(query_norm) * std::sqrt(embedding_norm);
  if (denominator == 0.0f) {
    return 0.0f;
  }
  return dot / denominator;
}

bool scored_note_order(const ScoredNote& left, const ScoredNote& right) {
  if (left.score != right.score) {
    return left.score > right.score;
  }
  return left.hit.rel_path < right.hit.rel_path;
}

}  // namespace

std::error_code upsert_ai_embedding_note_metadata(
    Database& db,
    const AiEmbeddingNoteMetadataRecord& metadata) {
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  sqlite3_stmt* stmt = nullptr;
  std::error_code ec = detail::prepare(
      db.connection,
      "INSERT INTO ai_embedding_cache("
      "  note_rel_path, note_title, absolute_path, created_at, updated_at, embedding"
      ") VALUES(?1, ?2, ?3, ?4, ?5, NULL) "
      "ON CONFLICT(note_rel_path) DO UPDATE SET "
      "  note_title=excluded.note_title,"
      "  absolute_path=excluded.absolute_path,"
      "  created_at=excluded.created_at,"
      "  updated_at=excluded.updated_at,"
      "  embedding=NULL;",
      &stmt);
  if (ec) {
    return ec;
  }

  sqlite3_bind_text(stmt, 1, metadata.rel_path.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, metadata.title.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, metadata.absolute_path.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 4, static_cast<sqlite3_int64>(metadata.created_at));
  sqlite3_bind_int64(stmt, 5, static_cast<sqlite3_int64>(metadata.updated_at));
  return detail::finalize_with_result(stmt, sqlite3_step(stmt));
}

std::error_code list_ai_embedding_note_timestamps(
    Database& db,
    std::vector<AiEmbeddingTimestampRecord>& out_records) {
  out_records.clear();
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  sqlite3_stmt* stmt = nullptr;
  std::error_code ec = detail::prepare(
      db.connection,
      "SELECT note_rel_path, updated_at "
      "FROM ai_embedding_cache "
      "ORDER BY note_rel_path ASC;",
      &stmt);
  if (ec) {
    return ec;
  }

  return detail::append_statement_rows(stmt, [&](sqlite3_stmt* row) {
    AiEmbeddingTimestampRecord record{};
    detail::assign_text_column(row, 0, record.rel_path);
    record.updated_at = static_cast<std::int64_t>(sqlite3_column_int64(row, 1));
    out_records.push_back(std::move(record));
  });
}

std::error_code update_ai_embedding(
    Database& db,
    std::string_view rel_path,
    const float* values,
    const std::size_t value_count) {
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  const std::string blob = serialize_embedding_blob(values, value_count);
  if (blob.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    return std::make_error_code(std::errc::value_too_large);
  }

  sqlite3_stmt* stmt = nullptr;
  std::error_code ec = detail::prepare(
      db.connection,
      "UPDATE ai_embedding_cache SET embedding=?1 WHERE note_rel_path=?2;",
      &stmt);
  if (ec) {
    return ec;
  }

  sqlite3_bind_blob(stmt, 1, blob.data(), static_cast<int>(blob.size()), SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, std::string(rel_path).c_str(), -1, SQLITE_TRANSIENT);
  return detail::finalize_with_result(stmt, sqlite3_step(stmt));
}

std::error_code clear_ai_embeddings(Database& db) {
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  sqlite3_stmt* stmt = nullptr;
  std::error_code ec =
      detail::prepare(db.connection, "UPDATE ai_embedding_cache SET embedding=NULL;", &stmt);
  if (ec) {
    return ec;
  }
  return detail::finalize_with_result(stmt, sqlite3_step(stmt));
}

std::error_code delete_ai_embedding_note(
    Database& db,
    std::string_view rel_path,
    bool& out_deleted) {
  out_deleted = false;
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  sqlite3_stmt* stmt = nullptr;
  std::error_code ec =
      detail::prepare(db.connection, "DELETE FROM ai_embedding_cache WHERE note_rel_path=?1;", &stmt);
  if (ec) {
    return ec;
  }

  sqlite3_bind_text(stmt, 1, std::string(rel_path).c_str(), -1, SQLITE_TRANSIENT);
  ec = detail::finalize_with_result(stmt, sqlite3_step(stmt));
  if (ec) {
    return ec;
  }
  out_deleted = sqlite3_changes(db.connection) > 0;
  return {};
}

std::error_code list_top_ai_embedding_notes(
    Database& db,
    const float* query_values,
    const std::size_t query_value_count,
    std::string_view exclude_rel_path,
    const std::size_t limit,
    std::vector<NoteListHit>& out_hits) {
  out_hits.clear();
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  sqlite3_stmt* stmt = nullptr;
  std::error_code ec = detail::prepare(
      db.connection,
      "SELECT note_rel_path, note_title, embedding "
      "FROM ai_embedding_cache "
      "WHERE embedding IS NOT NULL "
      "ORDER BY note_rel_path ASC;",
      &stmt);
  if (ec) {
    return ec;
  }

  std::vector<ScoredNote> candidates;
  std::vector<float> embedding;
  ec = detail::append_statement_rows(stmt, [&](sqlite3_stmt* row) {
    std::string rel_path;
    detail::assign_text_column(row, 0, rel_path);
    if (!exclude_rel_path.empty() && rel_path == exclude_rel_path) {
      return;
    }

    const void* blob = sqlite3_column_blob(row, 2);
    const int blob_size = sqlite3_column_bytes(row, 2);
    if (!parse_embedding_blob(blob, blob_size, embedding)) {
      return;
    }

    std::string title;
    detail::assign_text_column(row, 1, title);
    candidates.push_back(ScoredNote{
        NoteListHit{std::move(rel_path), std::move(title)},
        cosine_similarity(query_values, query_value_count, embedding)});
  });
  if (ec) {
    return ec;
  }

  std::sort(candidates.begin(), candidates.end(), scored_note_order);
  const std::size_t count = std::min(limit, candidates.size());
  out_hits.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    out_hits.push_back(std::move(candidates[index].hit));
  }
  return {};
}

}  // namespace kernel::storage
