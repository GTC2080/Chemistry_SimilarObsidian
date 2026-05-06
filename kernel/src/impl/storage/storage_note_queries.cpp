// Reason: This file owns note-oriented read queries and journal mirror writes.

#include "storage/storage_internal.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace kernel::storage {

namespace {

NoteListHit read_note_list_hit(sqlite3_stmt* stmt) {
  NoteListHit hit{};
  detail::assign_text_column(stmt, 0, hit.rel_path);
  detail::assign_text_column(stmt, 1, hit.title);
  return hit;
}

std::pair<std::string, std::string> ordered_graph_pair(
    const std::string& left,
    const std::string& right) {
  if (left < right) {
    return {left, right};
  }
  return {right, left};
}

std::string folder_from_rel_path(const std::string& rel_path) {
  const std::string::size_type split = rel_path.find_last_of("/\\");
  if (split == std::string::npos) {
    return {};
  }
  return rel_path.substr(0, split);
}

void add_graph_link(
    GraphRecord& graph,
    std::set<std::pair<std::string, std::string>>& seen_pairs,
    const std::string& source,
    const std::string& target,
    const char* kind) {
  if (source.empty() || target.empty() || source == target) {
    return;
  }

  const auto pair = ordered_graph_pair(source, target);
  if (!seen_pairs.insert(pair).second) {
    return;
  }

  graph.links.push_back(GraphLinkRecord{source, target, kind});
}

}  // namespace

std::error_code list_note_paths(Database& db, std::vector<std::string>& out_paths) {
  out_paths.clear();
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  sqlite3_stmt* stmt = nullptr;
  std::error_code ec = detail::prepare(
      db.connection,
      "SELECT rel_path FROM notes WHERE is_deleted=0 ORDER BY rel_path ASC;",
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

std::error_code list_note_catalog_records(
    Database& db,
    const std::size_t limit,
    std::vector<NoteCatalogRecord>& out_records) {
  out_records.clear();
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  sqlite3_stmt* stmt = nullptr;
  std::error_code ec = detail::prepare(
      db.connection,
      "SELECT rel_path, title, file_size, mtime_ns, content_revision "
      "FROM notes "
      "WHERE is_deleted=0 "
      "ORDER BY rel_path ASC "
      "LIMIT ?1;",
      &stmt);
  if (ec) {
    return ec;
  }

  sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(limit));
  return detail::append_statement_rows(stmt, [&](sqlite3_stmt* row) {
    NoteCatalogRecord record{};
    detail::assign_text_column(row, 0, record.rel_path);
    detail::assign_text_column(row, 1, record.title);
    record.file_size = static_cast<std::uint64_t>(sqlite3_column_int64(row, 2));
    record.mtime_ns = static_cast<std::uint64_t>(sqlite3_column_int64(row, 3));
    detail::assign_text_column(row, 4, record.content_revision);
    out_records.push_back(std::move(record));
  });
}

std::error_code count_active_notes(Database& db, std::uint64_t& out_count) {
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }
  return detail::scalar_count_query(
      db.connection,
      "SELECT COUNT(*) FROM notes WHERE is_deleted=0;",
      out_count);
}

std::error_code list_notes_by_tag(
    Database& db,
    std::string_view tag,
    const std::size_t limit,
    std::vector<NoteListHit>& out_hits) {
  out_hits.clear();
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  sqlite3_stmt* stmt = nullptr;
  std::error_code ec = detail::prepare(
      db.connection,
      "SELECT notes.rel_path, notes.title "
      "FROM note_tags "
      "JOIN notes ON notes.note_id = note_tags.note_id "
      "WHERE note_tags.tag = ?1 AND notes.is_deleted = 0 "
      "ORDER BY notes.rel_path ASC "
      "LIMIT ?2;",
      &stmt);
  if (ec) {
    return ec;
  }

  sqlite3_bind_text(stmt, 1, std::string(tag).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(limit));
  return detail::append_statement_rows(stmt, [&](sqlite3_stmt* row) {
    out_hits.push_back(read_note_list_hit(row));
  });
}

std::error_code list_tag_summaries(
    Database& db,
    const std::size_t limit,
    std::vector<TagSummaryRecord>& out_records) {
  out_records.clear();
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  sqlite3_stmt* stmt = nullptr;
  std::error_code ec = detail::prepare(
      db.connection,
      "SELECT note_tags.tag, COUNT(*) AS note_count "
      "FROM note_tags "
      "JOIN notes ON notes.note_id = note_tags.note_id "
      "WHERE notes.is_deleted = 0 "
      "GROUP BY note_tags.tag "
      "ORDER BY note_count DESC, note_tags.tag ASC "
      "LIMIT ?1;",
      &stmt);
  if (ec) {
    return ec;
  }

  sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(limit));
  return detail::append_statement_rows(stmt, [&](sqlite3_stmt* row) {
    TagSummaryRecord record{};
    detail::assign_text_column(row, 0, record.name);
    record.count = static_cast<std::uint32_t>(sqlite3_column_int(row, 1));
    out_records.push_back(std::move(record));
  });
}

std::error_code build_note_graph(
    Database& db,
    const std::size_t note_limit,
    GraphRecord& out_graph) {
  out_graph.nodes.clear();
  out_graph.links.clear();
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  sqlite3_stmt* note_stmt = nullptr;
  std::error_code ec = detail::prepare(
      db.connection,
      "SELECT rel_path, title "
      "FROM notes "
      "WHERE is_deleted = 0 "
      "ORDER BY rel_path ASC "
      "LIMIT ?1;",
      &note_stmt);
  if (ec) {
    return ec;
  }

  sqlite3_bind_int64(note_stmt, 1, static_cast<sqlite3_int64>(note_limit));

  std::set<std::string> node_ids;
  std::unordered_map<std::string, std::string> target_name_to_id;
  std::map<std::string, std::vector<std::string>> folder_groups;

  ec = detail::append_statement_rows(note_stmt, [&](sqlite3_stmt* row) {
    GraphNodeRecord node{};
    detail::assign_text_column(row, 0, node.id);
    detail::assign_text_column(row, 1, node.name);
    node.ghost = false;
    if (node.id.empty()) {
      return;
    }

    node_ids.insert(node.id);
    if (!node.name.empty()) {
      target_name_to_id.try_emplace(node.name, node.id);
    }
    target_name_to_id.try_emplace(detail::title_from_rel_path(node.id), node.id);
    folder_groups[folder_from_rel_path(node.id)].push_back(node.id);
    out_graph.nodes.push_back(std::move(node));
  });
  if (ec) {
    return ec;
  }

  std::set<std::pair<std::string, std::string>> seen_pairs;
  std::unordered_map<std::string, std::string> ghost_target_to_id;
  std::uint64_t ghost_index = 0;

  sqlite3_stmt* link_stmt = nullptr;
  ec = detail::prepare(
      db.connection,
      "WITH limited_notes AS ("
      "  SELECT note_id, rel_path "
      "  FROM notes "
      "  WHERE is_deleted = 0 "
      "  ORDER BY rel_path ASC "
      "  LIMIT ?1"
      ") "
      "SELECT src.rel_path, note_links.target "
      "FROM note_links "
      "JOIN limited_notes AS src ON src.note_id = note_links.note_id "
      "ORDER BY src.rel_path ASC, note_links.target ASC;",
      &link_stmt);
  if (ec) {
    return ec;
  }

  sqlite3_bind_int64(link_stmt, 1, static_cast<sqlite3_int64>(note_limit));

  ec = detail::append_statement_rows(link_stmt, [&](sqlite3_stmt* row) {
    std::string source;
    std::string target_name;
    detail::assign_text_column(row, 0, source);
    detail::assign_text_column(row, 1, target_name);
    if (!node_ids.contains(source) || target_name.empty()) {
      return;
    }

    std::string target_id;
    if (const auto live_it = target_name_to_id.find(target_name);
        live_it != target_name_to_id.end()) {
      target_id = live_it->second;
    } else if (const auto ghost_it = ghost_target_to_id.find(target_name);
               ghost_it != ghost_target_to_id.end()) {
      target_id = ghost_it->second;
    } else {
      ++ghost_index;
      target_id = "__ghost_" + std::to_string(ghost_index);
      ghost_target_to_id.emplace(target_name, target_id);
      node_ids.insert(target_id);
      out_graph.nodes.push_back(GraphNodeRecord{target_id, target_name, true});
    }

    add_graph_link(out_graph, seen_pairs, source, target_id, "link");
  });
  if (ec) {
    return ec;
  }

  sqlite3_stmt* tag_stmt = nullptr;
  ec = detail::prepare(
      db.connection,
      "WITH limited_notes AS ("
      "  SELECT note_id, rel_path "
      "  FROM notes "
      "  WHERE is_deleted = 0 "
      "  ORDER BY rel_path ASC "
      "  LIMIT ?1"
      ") "
      "SELECT DISTINCT left_note.rel_path, right_note.rel_path "
      "FROM note_tags AS left_tag "
      "JOIN note_tags AS right_tag "
      "  ON left_tag.tag = right_tag.tag AND left_tag.note_id < right_tag.note_id "
      "JOIN limited_notes AS left_note ON left_note.note_id = left_tag.note_id "
      "JOIN limited_notes AS right_note ON right_note.note_id = right_tag.note_id "
      "ORDER BY left_note.rel_path ASC, right_note.rel_path ASC;",
      &tag_stmt);
  if (ec) {
    return ec;
  }

  sqlite3_bind_int64(tag_stmt, 1, static_cast<sqlite3_int64>(note_limit));

  ec = detail::append_statement_rows(tag_stmt, [&](sqlite3_stmt* row) {
    std::string left;
    std::string right;
    detail::assign_text_column(row, 0, left);
    detail::assign_text_column(row, 1, right);
    if (!node_ids.contains(left) || !node_ids.contains(right)) {
      return;
    }
    add_graph_link(out_graph, seen_pairs, left, right, "tag");
  });
  if (ec) {
    return ec;
  }

  for (auto& [folder, ids] : folder_groups) {
    (void)folder;
    if (ids.size() < 2) {
      continue;
    }
    std::sort(ids.begin(), ids.end());
    if (ids.size() <= 15) {
      for (std::size_t left = 0; left < ids.size(); ++left) {
        for (std::size_t right = left + 1; right < ids.size(); ++right) {
          add_graph_link(out_graph, seen_pairs, ids[left], ids[right], "folder");
        }
      }
      continue;
    }

    for (std::size_t index = 1; index < ids.size(); ++index) {
      add_graph_link(out_graph, seen_pairs, ids[index - 1], ids[index], "folder");
    }
  }

  return {};
}

std::error_code list_backlinks_for_rel_path(
    Database& db,
    std::string_view rel_path,
    const std::size_t limit,
    std::vector<NoteListHit>& out_hits) {
  out_hits.clear();
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  const std::string fallback_title = detail::title_from_rel_path(rel_path);

  sqlite3_stmt* stmt = nullptr;
  std::error_code ec = detail::prepare(
      db.connection,
      "SELECT src.rel_path, src.title "
      "FROM notes AS dst "
      "JOIN note_links ON note_links.target = dst.title OR note_links.target = ?2 "
      "JOIN notes AS src ON src.note_id = note_links.note_id "
      "WHERE dst.rel_path = ?1 AND dst.is_deleted = 0 AND src.is_deleted = 0 "
      "GROUP BY src.note_id, src.rel_path, src.title "
      "ORDER BY src.rel_path ASC "
      "LIMIT ?3;",
      &stmt);
  if (ec) {
    return ec;
  }

  sqlite3_bind_text(stmt, 1, std::string(rel_path).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, fallback_title.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(limit));
  return detail::append_statement_rows(stmt, [&](sqlite3_stmt* row) {
    out_hits.push_back(read_note_list_hit(row));
  });
}

std::error_code insert_journal_state(
    Database& db,
    std::string_view op_id,
    std::string_view op_type,
    std::string_view rel_path,
    std::string_view temp_path,
    std::string_view phase) {
  if (db.connection == nullptr) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }

  sqlite3_stmt* stmt = nullptr;
  std::error_code ec = detail::prepare(
      db.connection,
      "INSERT INTO journal_state(op_id, op_type, rel_path, temp_path, phase, recorded_at_ns) "
      "VALUES(?1, ?2, ?3, ?4, ?5, ?6);",
      &stmt);
  if (ec) {
    return ec;
  }

  sqlite3_bind_text(stmt, 1, std::string(op_id).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, std::string(op_type).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, std::string(rel_path).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, std::string(temp_path).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 5, std::string(phase).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 6, static_cast<sqlite3_int64>(detail::now_ns()));
  return detail::finalize_with_result(stmt, sqlite3_step(stmt));
}

}  // namespace kernel::storage
