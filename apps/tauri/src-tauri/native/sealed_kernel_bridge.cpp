#include "sealed_kernel_bridge.h"

#include "kernel/c_api.h"

#ifdef _WIN32
#include <Windows.h>
#endif

#include <cstdlib>
#include <cstring>
#include <string>

struct sealed_kernel_bridge_session {
  kernel_handle* handle = nullptr;
};

namespace {

char* CopyString(const std::string& value) {
  auto* out = static_cast<char*>(std::malloc(value.size() + 1));
  if (out == nullptr) {
    return nullptr;
  }
  std::memcpy(out, value.c_str(), value.size() + 1);
  return out;
}

void SetError(char** out_error, const std::string& message) {
  if (out_error == nullptr) {
    return;
  }
  *out_error = CopyString(message);
}

const char* KernelErrorCodeName(const kernel_error_code code) {
  switch (code) {
    case KERNEL_OK:
      return "KERNEL_OK";
    case KERNEL_ERROR_INVALID_ARGUMENT:
      return "KERNEL_ERROR_INVALID_ARGUMENT";
    case KERNEL_ERROR_NOT_FOUND:
      return "KERNEL_ERROR_NOT_FOUND";
    case KERNEL_ERROR_CONFLICT:
      return "KERNEL_ERROR_CONFLICT";
    case KERNEL_ERROR_IO:
      return "KERNEL_ERROR_IO";
    case KERNEL_ERROR_INTERNAL:
      return "KERNEL_ERROR_INTERNAL";
    case KERNEL_ERROR_TIMEOUT:
      return "KERNEL_ERROR_TIMEOUT";
  }

  return "KERNEL_ERROR_UNKNOWN";
}

std::string Utf8ToActiveCodePage(const char* value) {
  if (value == nullptr || value[0] == '\0') {
    return {};
  }

#ifdef _WIN32
  const int wide_size =
      MultiByteToWideChar(CP_UTF8, 0, value, -1, nullptr, 0);
  if (wide_size <= 0) {
    return {};
  }

  std::wstring wide_value(static_cast<std::size_t>(wide_size), L'\0');
  MultiByteToWideChar(
      CP_UTF8,
      0,
      value,
      -1,
      wide_value.data(),
      wide_size);

  const int acp_size =
      WideCharToMultiByte(CP_ACP, 0, wide_value.c_str(), -1, nullptr, 0, nullptr, nullptr);
  if (acp_size <= 0) {
    return {};
  }

  std::string acp_value(static_cast<std::size_t>(acp_size), '\0');
  WideCharToMultiByte(
      CP_ACP,
      0,
      wide_value.c_str(),
      -1,
      acp_value.data(),
      acp_size,
      nullptr,
      nullptr);
  if (!acp_value.empty() && acp_value.back() == '\0') {
    acp_value.pop_back();
  }
  return acp_value;
#else
  return std::string(value);
#endif
}

std::string ActiveCodePageToUtf8(const char* value) {
  if (value == nullptr || value[0] == '\0') {
    return {};
  }

#ifdef _WIN32
  const int wide_size =
      MultiByteToWideChar(CP_ACP, 0, value, -1, nullptr, 0);
  if (wide_size <= 0) {
    return {};
  }

  std::wstring wide_value(static_cast<std::size_t>(wide_size), L'\0');
  MultiByteToWideChar(CP_ACP, 0, value, -1, wide_value.data(), wide_size);

  const int utf8_size =
      WideCharToMultiByte(CP_UTF8, 0, wide_value.c_str(), -1, nullptr, 0, nullptr, nullptr);
  if (utf8_size <= 0) {
    return {};
  }

  std::string utf8_value(static_cast<std::size_t>(utf8_size), '\0');
  WideCharToMultiByte(
      CP_UTF8,
      0,
      wide_value.c_str(),
      -1,
      utf8_value.data(),
      utf8_size,
      nullptr,
      nullptr);
  if (!utf8_value.empty() && utf8_value.back() == '\0') {
    utf8_value.pop_back();
  }
  return utf8_value;
#else
  return std::string(value);
#endif
}

int32_t ReturnKernelError(
    const kernel_status status,
    const char* operation,
    char** out_error) {
  const char* code_name = KernelErrorCodeName(status.code);
  SetError(out_error, std::string(operation) + " failed (" + code_name + ").");
  return static_cast<int32_t>(status.code);
}

std::string JsonEscape(const char* value) {
  if (value == nullptr) {
    return "";
  }

  std::string escaped;
  for (const unsigned char ch : std::string(value)) {
    switch (ch) {
      case '\"':
        escaped += "\\\"";
        break;
      case '\\':
        escaped += "\\\\";
        break;
      case '\b':
        escaped += "\\b";
        break;
      case '\f':
        escaped += "\\f";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        if (ch < 0x20) {
          constexpr char hex[] = "0123456789abcdef";
          escaped += "\\u00";
          escaped += hex[(ch >> 4) & 0x0F];
          escaped += hex[ch & 0x0F];
        } else {
          escaped.push_back(static_cast<char>(ch));
        }
        break;
    }
  }
  return escaped;
}

void AppendNoteHitJson(
    std::string& json,
    const char* rel_path,
    const char* title,
    uint64_t mtime_ns = 0) {
  const std::string rel_path_utf8 = ActiveCodePageToUtf8(rel_path);
  json += "{\"rel_path\":\"" + JsonEscape(rel_path_utf8.c_str()) + "\",";
  json += "\"title\":\"" + JsonEscape(title) + "\",";
  json += "\"mtime_ns\":" + std::to_string(mtime_ns) + "}";
}

void AppendFileTreeNoteJson(std::string& json, const kernel_file_tree_note& note) {
  const std::string rel_path_utf8 = ActiveCodePageToUtf8(note.rel_path);
  const std::string name_utf8 = ActiveCodePageToUtf8(note.name);
  json += "{\"relPath\":\"" + JsonEscape(rel_path_utf8.c_str()) + "\",";
  json += "\"name\":\"" + JsonEscape(name_utf8.c_str()) + "\",";
  json += "\"extension\":\"" + JsonEscape(note.extension) + "\",";
  json += "\"mtimeNs\":" + std::to_string(note.mtime_ns) + "}";
}

void AppendFileTreeNodeJson(std::string& json, const kernel_file_tree_node& node) {
  const std::string name_utf8 = ActiveCodePageToUtf8(node.name);
  const std::string full_name_utf8 = ActiveCodePageToUtf8(node.full_name);
  const std::string relative_path_utf8 = ActiveCodePageToUtf8(node.relative_path);

  json += "{\"name\":\"" + JsonEscape(name_utf8.c_str()) + "\",";
  json += "\"fullName\":\"" + JsonEscape(full_name_utf8.c_str()) + "\",";
  json += "\"relativePath\":\"" + JsonEscape(relative_path_utf8.c_str()) + "\",";
  json += "\"isFolder\":";
  json += node.is_folder != 0 ? "true" : "false";
  json += ",\"note\":";
  if (node.has_note != 0) {
    AppendFileTreeNoteJson(json, node.note);
  } else {
    json += "null";
  }
  json += ",\"children\":[";
  for (size_t index = 0; index < node.child_count; ++index) {
    if (index != 0) {
      json += ",";
    }
    AppendFileTreeNodeJson(json, node.children[index]);
  }
  json += "],\"fileCount\":" + std::to_string(node.file_count) + "}";
}

}  // namespace

char* sealed_kernel_bridge_info_json(void) {
  return CopyString(
      "{\"adapter\":\"sealed-kernel-cpp-bridge\","
      "\"kernel\":\"chem_kernel\","
      "\"link_mode\":\"static-lib\","
      "\"path_encoding\":\"utf8-to-active-code-page\"}");
}

void sealed_kernel_bridge_free_string(char* value) {
  std::free(value);
}

int32_t sealed_kernel_bridge_open_vault_utf8(
    const char* vault_path_utf8,
    sealed_kernel_bridge_session** out_session,
    char** out_error) {
  if (out_session == nullptr) {
    SetError(out_error, "sealed_kernel_bridge_open_vault_utf8 missing out_session.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  *out_session = nullptr;

  const std::string vault_path = Utf8ToActiveCodePage(vault_path_utf8);
  if (vault_path.empty()) {
    SetError(out_error, "vault_path must be a non-empty UTF-8 string.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_handle* handle = nullptr;
  const kernel_status status = kernel_open_vault(vault_path.c_str(), &handle);
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_open_vault", out_error);
  }

  auto* session = new sealed_kernel_bridge_session();
  session->handle = handle;
  *out_session = session;
  return static_cast<int32_t>(KERNEL_OK);
}

void sealed_kernel_bridge_close(sealed_kernel_bridge_session* session) {
  if (session == nullptr) {
    return;
  }

  if (session->handle != nullptr) {
    kernel_close(session->handle);
    session->handle = nullptr;
  }
  delete session;
}

int32_t sealed_kernel_bridge_get_state(
    sealed_kernel_bridge_session* session,
    sealed_kernel_bridge_state_snapshot* out_state,
    char** out_error) {
  if (session == nullptr || session->handle == nullptr || out_state == nullptr) {
    SetError(out_error, "sealed kernel session is not open.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_state_snapshot snapshot{};
  const kernel_status status = kernel_get_state(session->handle, &snapshot);
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_get_state", out_error);
  }

  out_state->session_state = static_cast<int32_t>(snapshot.session_state);
  out_state->index_state = static_cast<int32_t>(snapshot.index_state);
  out_state->indexed_note_count = snapshot.indexed_note_count;
  out_state->pending_recovery_ops = snapshot.pending_recovery_ops;
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_query_notes_json(
    sealed_kernel_bridge_session* session,
    uint64_t limit,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (session == nullptr || session->handle == nullptr || out_json == nullptr || limit == 0) {
    SetError(out_error, "sealed kernel session is not open or note query arguments are invalid.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_note_list notes{};
  const kernel_status status =
      kernel_query_notes(session->handle, static_cast<size_t>(limit), &notes);
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_query_notes", out_error);
  }

  std::string json = "{\"notes\":[";
  for (size_t index = 0; index < notes.count; ++index) {
    if (index != 0) {
      json += ",";
    }
    const kernel_note_record& note = notes.notes[index];
    const std::string rel_path_utf8 = ActiveCodePageToUtf8(note.rel_path);
    json += "{\"rel_path\":\"" + JsonEscape(rel_path_utf8.c_str()) + "\",";
    json += "\"title\":\"" + JsonEscape(note.title) + "\",";
    json += "\"file_size\":" + std::to_string(note.file_size) + ",";
    json += "\"mtime_ns\":" + std::to_string(note.mtime_ns) + ",";
    json += "\"content_revision\":\"" + JsonEscape(note.content_revision) + "\"}";
  }
  json += "],\"count\":" + std::to_string(notes.count) + "}";

  kernel_free_note_list(&notes);
  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "failed to allocate note catalog JSON.");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_query_file_tree_json(
    sealed_kernel_bridge_session* session,
    uint64_t limit,
    const char* ignored_roots_utf8,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (session == nullptr || session->handle == nullptr || out_json == nullptr || limit == 0) {
    SetError(out_error, "sealed kernel session is not open or file tree arguments are invalid.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_file_tree tree{};
  const std::string ignored_roots = Utf8ToActiveCodePage(ignored_roots_utf8);
  const kernel_status status = kernel_query_file_tree_filtered(
      session->handle,
      static_cast<size_t>(limit),
      ignored_roots.empty() ? nullptr : ignored_roots.c_str(),
      &tree);
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_query_file_tree_filtered", out_error);
  }

  std::string json = "{\"nodes\":[";
  for (size_t index = 0; index < tree.count; ++index) {
    if (index != 0) {
      json += ",";
    }
    AppendFileTreeNodeJson(json, tree.nodes[index]);
  }
  json += "],\"count\":" + std::to_string(tree.count) + "}";

  kernel_free_file_tree(&tree);
  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "failed to allocate file tree JSON.");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_read_note_json(
    sealed_kernel_bridge_session* session,
    const char* rel_path_utf8,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (session == nullptr || session->handle == nullptr || out_json == nullptr) {
    SetError(out_error, "sealed kernel session is not open.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string rel_path = Utf8ToActiveCodePage(rel_path_utf8);
  if (rel_path.empty()) {
    SetError(out_error, "rel_path must be a non-empty UTF-8 string.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_owned_buffer buffer{};
  kernel_note_metadata metadata{};
  const kernel_status status =
      kernel_read_note(session->handle, rel_path.c_str(), &buffer, &metadata);
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_read_note", out_error);
  }

  const std::string content(buffer.data == nullptr ? "" : std::string(buffer.data, buffer.size));
  std::string json = "{\"content\":\"" + JsonEscape(content.c_str()) + "\",";
  json += "\"metadata\":{\"file_size\":" + std::to_string(metadata.file_size) + ",";
  json += "\"mtime_ns\":" + std::to_string(metadata.mtime_ns) + ",";
  json += "\"content_revision\":\"" + JsonEscape(metadata.content_revision) + "\"}}";
  kernel_free_buffer(&buffer);

  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "failed to allocate note read JSON.");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_write_note_json(
    sealed_kernel_bridge_session* session,
    const char* rel_path_utf8,
    const char* content_utf8,
    uint64_t content_size,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (session == nullptr || session->handle == nullptr || out_json == nullptr ||
      content_utf8 == nullptr) {
    SetError(out_error, "sealed kernel session is not open or write arguments are invalid.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string rel_path = Utf8ToActiveCodePage(rel_path_utf8);
  if (rel_path.empty()) {
    SetError(out_error, "rel_path must be a non-empty UTF-8 string.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_note_metadata current_metadata{};
  kernel_owned_buffer current_buffer{};
  const kernel_status read_status =
      kernel_read_note(session->handle, rel_path.c_str(), &current_buffer, &current_metadata);
  const bool current_exists = read_status.code == KERNEL_OK;
  if (current_exists) {
    kernel_free_buffer(&current_buffer);
  } else if (read_status.code != KERNEL_ERROR_NOT_FOUND) {
    return ReturnKernelError(read_status, "kernel_read_note", out_error);
  }

  kernel_note_metadata written_metadata{};
  kernel_write_disposition disposition{};
  const char* expected_revision = current_exists ? current_metadata.content_revision : nullptr;
  const kernel_status write_status = kernel_write_note(
      session->handle,
      rel_path.c_str(),
      content_utf8,
      static_cast<size_t>(content_size),
      expected_revision,
      &written_metadata,
      &disposition);
  if (write_status.code != KERNEL_OK) {
    return ReturnKernelError(write_status, "kernel_write_note", out_error);
  }

  std::string json = "{\"metadata\":{\"file_size\":" + std::to_string(written_metadata.file_size) + ",";
  json += "\"mtime_ns\":" + std::to_string(written_metadata.mtime_ns) + ",";
  json += "\"content_revision\":\"" + JsonEscape(written_metadata.content_revision) + "\"},";
  json += "\"disposition\":" + std::to_string(static_cast<int>(disposition)) + "}";

  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "failed to allocate note write JSON.");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_query_search_notes_json(
    sealed_kernel_bridge_session* session,
    const char* query_utf8,
    uint64_t limit,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (session == nullptr || session->handle == nullptr || out_json == nullptr ||
      query_utf8 == nullptr || query_utf8[0] == '\0' || limit == 0) {
    SetError(out_error, "sealed kernel session is not open or query search arguments are invalid.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_search_query request{};
  request.query = query_utf8;
  request.limit = static_cast<size_t>(limit);
  request.offset = 0;
  request.kind = KERNEL_SEARCH_KIND_NOTE;
  request.tag_filter = nullptr;
  request.path_prefix = nullptr;
  request.include_deleted = 0;
  request.sort_mode = KERNEL_SEARCH_SORT_RANK_V1;

  kernel_search_page page{};
  const kernel_status status =
      kernel_query_search(session->handle, &request, &page);
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_query_search", out_error);
  }

  std::string json = "{\"notes\":[";
  for (size_t index = 0; index < page.count; ++index) {
    if (index != 0) {
      json += ",";
    }
    AppendNoteHitJson(json, page.hits[index].rel_path, page.hits[index].title);
  }
  json += "],\"count\":" + std::to_string(page.count) + "}";

  kernel_free_search_page(&page);
  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "failed to allocate query search JSON.");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_query_tags_json(
    sealed_kernel_bridge_session* session,
    uint64_t limit,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (session == nullptr || session->handle == nullptr || out_json == nullptr || limit == 0) {
    SetError(out_error, "sealed kernel session is not open or tag arguments are invalid.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_tag_list tags{};
  const kernel_status status =
      kernel_query_tags(session->handle, static_cast<size_t>(limit), &tags);
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_query_tags", out_error);
  }

  std::string json = "{\"tags\":[";
  for (size_t index = 0; index < tags.count; ++index) {
    if (index != 0) {
      json += ",";
    }
    json += "{\"name\":\"" + JsonEscape(tags.tags[index].name) + "\",";
    json += "\"count\":" + std::to_string(tags.tags[index].count) + "}";
  }
  json += "],\"count\":" + std::to_string(tags.count) + "}";

  kernel_free_tag_list(&tags);
  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "failed to allocate tag summary JSON.");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_query_tag_notes_json(
    sealed_kernel_bridge_session* session,
    const char* tag_utf8,
    uint64_t limit,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (session == nullptr || session->handle == nullptr || out_json == nullptr ||
      tag_utf8 == nullptr || tag_utf8[0] == '\0' || limit == 0) {
    SetError(out_error, "sealed kernel session is not open or tag-note arguments are invalid.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_search_results results{};
  const kernel_status status =
      kernel_query_tag_notes(session->handle, tag_utf8, static_cast<size_t>(limit), &results);
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_query_tag_notes", out_error);
  }

  std::string json = "{\"notes\":[";
  for (size_t index = 0; index < results.count; ++index) {
    if (index != 0) {
      json += ",";
    }
    AppendNoteHitJson(json, results.hits[index].rel_path, results.hits[index].title);
  }
  json += "],\"count\":" + std::to_string(results.count) + "}";

  kernel_free_search_results(&results);
  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "failed to allocate tag-note JSON.");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_query_graph_json(
    sealed_kernel_bridge_session* session,
    uint64_t limit,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (session == nullptr || session->handle == nullptr || out_json == nullptr || limit == 0) {
    SetError(out_error, "sealed kernel session is not open or graph arguments are invalid.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_graph graph{};
  const kernel_status status =
      kernel_query_graph(session->handle, static_cast<size_t>(limit), &graph);
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_query_graph", out_error);
  }

  std::string json = "{\"nodes\":[";
  for (size_t index = 0; index < graph.node_count; ++index) {
    if (index != 0) {
      json += ",";
    }
    const std::string node_id_utf8 = ActiveCodePageToUtf8(graph.nodes[index].id);
    json += "{\"id\":\"" + JsonEscape(node_id_utf8.c_str()) + "\",";
    json += "\"name\":\"" + JsonEscape(graph.nodes[index].name) + "\",";
    json += "\"ghost\":";
    json += graph.nodes[index].ghost != 0 ? "true" : "false";
    json += "}";
  }
  json += "],\"links\":[";
  for (size_t index = 0; index < graph.link_count; ++index) {
    if (index != 0) {
      json += ",";
    }
    const std::string source_utf8 = ActiveCodePageToUtf8(graph.links[index].source);
    const std::string target_utf8 = ActiveCodePageToUtf8(graph.links[index].target);
    json += "{\"source\":\"" + JsonEscape(source_utf8.c_str()) + "\",";
    json += "\"target\":\"" + JsonEscape(target_utf8.c_str()) + "\",";
    json += "\"kind\":\"" + JsonEscape(graph.links[index].kind) + "\"}";
  }
  json += "]}";

  kernel_free_graph(&graph);
  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "failed to allocate graph JSON.");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_query_backlinks_json(
    sealed_kernel_bridge_session* session,
    const char* rel_path_utf8,
    uint64_t limit,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (session == nullptr || session->handle == nullptr || out_json == nullptr ||
      rel_path_utf8 == nullptr || rel_path_utf8[0] == '\0' || limit == 0) {
    SetError(out_error, "sealed kernel session is not open or backlink arguments are invalid.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string rel_path = Utf8ToActiveCodePage(rel_path_utf8);
  if (rel_path.empty()) {
    SetError(out_error, "rel_path must be a non-empty UTF-8 string.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_search_results results{};
  const kernel_status status = kernel_query_backlinks(
      session->handle,
      rel_path.c_str(),
      static_cast<size_t>(limit),
      &results);
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_query_backlinks", out_error);
  }

  std::string json = "{\"notes\":[";
  for (size_t index = 0; index < results.count; ++index) {
    if (index != 0) {
      json += ",";
    }
    AppendNoteHitJson(json, results.hits[index].rel_path, results.hits[index].title);
  }
  json += "],\"count\":" + std::to_string(results.count) + "}";

  kernel_free_search_results(&results);
  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "failed to allocate backlink JSON.");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_create_folder(
    sealed_kernel_bridge_session* session,
    const char* folder_rel_path_utf8,
    char** out_error) {
  if (session == nullptr || session->handle == nullptr) {
    SetError(out_error, "sealed kernel session is not open.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string folder_rel_path = Utf8ToActiveCodePage(folder_rel_path_utf8);
  if (folder_rel_path.empty()) {
    SetError(out_error, "folder_rel_path must be a non-empty UTF-8 string.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const kernel_status status =
      kernel_create_folder(session->handle, folder_rel_path.c_str());
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_create_folder", out_error);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_delete_entry(
    sealed_kernel_bridge_session* session,
    const char* target_rel_path_utf8,
    char** out_error) {
  if (session == nullptr || session->handle == nullptr) {
    SetError(out_error, "sealed kernel session is not open.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string target_rel_path = Utf8ToActiveCodePage(target_rel_path_utf8);
  if (target_rel_path.empty()) {
    SetError(out_error, "target_rel_path must be a non-empty UTF-8 string.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const kernel_status status =
      kernel_delete_entry(session->handle, target_rel_path.c_str());
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_delete_entry", out_error);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_rename_entry(
    sealed_kernel_bridge_session* session,
    const char* source_rel_path_utf8,
    const char* new_name_utf8,
    char** out_error) {
  if (session == nullptr || session->handle == nullptr) {
    SetError(out_error, "sealed kernel session is not open.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string source_rel_path = Utf8ToActiveCodePage(source_rel_path_utf8);
  const std::string new_name = Utf8ToActiveCodePage(new_name_utf8);
  if (source_rel_path.empty() || new_name.empty()) {
    SetError(out_error, "source_rel_path and new_name must be non-empty UTF-8 strings.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const kernel_status status =
      kernel_rename_entry(session->handle, source_rel_path.c_str(), new_name.c_str());
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_rename_entry", out_error);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_move_entry(
    sealed_kernel_bridge_session* session,
    const char* source_rel_path_utf8,
    const char* dest_folder_rel_path_utf8,
    char** out_error) {
  if (session == nullptr || session->handle == nullptr) {
    SetError(out_error, "sealed kernel session is not open.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string source_rel_path = Utf8ToActiveCodePage(source_rel_path_utf8);
  const std::string dest_folder_rel_path =
      dest_folder_rel_path_utf8 == nullptr ? std::string() : Utf8ToActiveCodePage(dest_folder_rel_path_utf8);
  if (source_rel_path.empty()) {
    SetError(out_error, "source_rel_path must be a non-empty UTF-8 string.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const kernel_status status = kernel_move_entry(
      session->handle,
      source_rel_path.c_str(),
      dest_folder_rel_path.empty() ? nullptr : dest_folder_rel_path.c_str());
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_move_entry", out_error);
  }
  return static_cast<int32_t>(KERNEL_OK);
}
