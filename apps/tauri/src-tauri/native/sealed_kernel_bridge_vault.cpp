#include "sealed_kernel_bridge_internal.h"

using namespace sealed_kernel_bridge_internal;

int32_t sealed_kernel_bridge_get_note_catalog_default_limit(
    uint64_t* out_limit,
    char** out_error) {
  if (out_limit == nullptr) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  size_t limit = 0;
  const kernel_status status = kernel_get_note_catalog_default_limit(&limit);
  if (status.code != KERNEL_OK) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(status.code);
  }

  *out_limit = static_cast<uint64_t>(limit);
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_get_note_query_default_limit(
    uint64_t* out_limit,
    char** out_error) {
  if (out_limit == nullptr) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  size_t limit = 0;
  const kernel_status status = kernel_get_note_query_default_limit(&limit);
  if (status.code != KERNEL_OK) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(status.code);
  }

  *out_limit = static_cast<uint64_t>(limit);
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_get_vault_scan_default_limit(
    uint64_t* out_limit,
    char** out_error) {
  if (out_limit == nullptr) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  size_t limit = 0;
  const kernel_status status = kernel_get_vault_scan_default_limit(&limit);
  if (status.code != KERNEL_OK) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(status.code);
  }

  *out_limit = static_cast<uint64_t>(limit);
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t QueryNotesJson(
    sealed_kernel_bridge_session* session,
    uint64_t limit,
    const char* ignored_roots_utf8,
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
  const std::string ignored_roots = Utf8ToActiveCodePage(ignored_roots_utf8);
  const kernel_status status = ignored_roots.empty()
      ? kernel_query_notes(session->handle, static_cast<size_t>(limit), &notes)
      : kernel_query_notes_filtered(
            session->handle,
            static_cast<size_t>(limit),
            ignored_roots.c_str(),
            &notes);
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(
        status,
        ignored_roots.empty() ? "kernel_query_notes" : "kernel_query_notes_filtered",
        out_error);
  }

  const std::string json = NoteCatalogJson(notes);
  kernel_free_note_list(&notes);
  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "failed to allocate note catalog JSON.");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_query_notes_json(
    sealed_kernel_bridge_session* session,
    uint64_t limit,
    char** out_json,
    char** out_error) {
  return QueryNotesJson(session, limit, nullptr, out_json, out_error);
}

int32_t sealed_kernel_bridge_query_notes_filtered_json(
    sealed_kernel_bridge_session* session,
    uint64_t limit,
    const char* ignored_roots_utf8,
    char** out_json,
    char** out_error) {
  return QueryNotesJson(session, limit, ignored_roots_utf8, out_json, out_error);
}

int32_t sealed_kernel_bridge_query_changed_notes_json(
    sealed_kernel_bridge_session* session,
    const char* changed_paths_lf_utf8,
    uint64_t limit,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (session == nullptr || session->handle == nullptr || out_json == nullptr || limit == 0) {
    SetError(
        out_error,
        "sealed kernel session is not open or changed note query arguments are invalid.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string changed_paths = Utf8ToActiveCodePage(changed_paths_lf_utf8);
  kernel_note_list notes{};
  const kernel_status status = kernel_query_changed_notes(
      session->handle,
      changed_paths.c_str(),
      static_cast<size_t>(limit),
      &notes);
  if (status.code != KERNEL_OK) {
    kernel_free_note_list(&notes);
    return ReturnKernelError(status, "kernel_query_changed_notes", out_error);
  }

  const std::string json = NoteCatalogJson(notes);
  kernel_free_note_list(&notes);
  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "failed to allocate changed note catalog JSON.");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_get_file_tree_default_limit(
    uint64_t* out_limit,
    char** out_error) {
  if (out_limit == nullptr) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  size_t limit = 0;
  const kernel_status status = kernel_get_file_tree_default_limit(&limit);
  if (status.code != KERNEL_OK) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(status.code);
  }

  *out_limit = static_cast<uint64_t>(limit);
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

int32_t sealed_kernel_bridge_filter_supported_vault_paths_filtered_json(
    const char* changed_paths_lf_utf8,
    const char* ignored_roots_utf8,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (out_json == nullptr) {
    SetError(out_error, "filtered supported vault path output pointer is invalid.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string changed_paths = Utf8ToActiveCodePage(changed_paths_lf_utf8);
  const std::string ignored_roots = Utf8ToActiveCodePage(ignored_roots_utf8);
  kernel_path_list paths{};
  const kernel_status status = kernel_filter_supported_vault_paths_filtered(
      changed_paths.c_str(),
      ignored_roots.c_str(),
      &paths);
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_filter_supported_vault_paths_filtered", out_error);
  }

  const std::string json = PathListToJson(paths);
  kernel_free_path_list(&paths);
  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "failed to allocate filtered supported vault path JSON.");
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

int32_t sealed_kernel_bridge_read_vault_file_bytes(
    sealed_kernel_bridge_session* session,
    const char* host_path_utf8,
    uint64_t host_path_size,
    uint8_t** out_bytes,
    uint64_t* out_size,
    char** out_error) {
  if (out_bytes != nullptr) {
    *out_bytes = nullptr;
  }
  if (out_size != nullptr) {
    *out_size = 0;
  }
  if (
      session == nullptr || session->handle == nullptr || out_bytes == nullptr ||
      out_size == nullptr || (host_path_size > 0 && host_path_utf8 == nullptr) ||
      host_path_size > (std::numeric_limits<std::size_t>::max)()) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string host_path_utf8_value =
      host_path_utf8 == nullptr
          ? std::string()
          : std::string(host_path_utf8, static_cast<std::size_t>(host_path_size));
  const std::string host_path = Utf8ToActiveCodePage(host_path_utf8_value.c_str());
  kernel_owned_buffer buffer{};
  const kernel_status status = kernel_read_vault_file(
      session->handle,
      host_path.c_str(),
      host_path.size(),
      &buffer);
  if (status.code != KERNEL_OK) {
    kernel_free_buffer(&buffer);
    return ReturnKernelError(status, "kernel_read_vault_file", out_error);
  }

  auto* copied = CopyBytes(buffer.data, buffer.size);
  const std::size_t copied_size = buffer.size;
  kernel_free_buffer(&buffer);
  if (copied == nullptr && copied_size != 0) {
    SetError(out_error, "allocation_failed");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }

  *out_bytes = copied;
  *out_size = static_cast<uint64_t>(copied_size);
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

  kernel_search_results results{};
  const kernel_status status =
      kernel_search_notes_limited(session->handle, query_utf8, static_cast<size_t>(limit), &results);
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_search_notes_limited", out_error);
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
    SetError(out_error, "failed to allocate query search JSON.");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}


int32_t sealed_kernel_bridge_read_first_changed_markdown_note_content_text(
    sealed_kernel_bridge_session* session,
    const char* changed_paths_lf_utf8,
    char** out_text,
    char** out_error) {
  if (out_text != nullptr) {
    *out_text = nullptr;
  }
  if (session == nullptr || session->handle == nullptr || out_text == nullptr) {
    SetError(out_error, "sealed kernel session is not open.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string changed_paths = Utf8ToActiveCodePage(changed_paths_lf_utf8);
  kernel_owned_buffer buffer{};
  const kernel_status status = kernel_read_first_changed_markdown_note_content(
      session->handle,
      changed_paths.c_str(),
      &buffer);
  if (status.code != KERNEL_OK) {
    kernel_free_buffer(&buffer);
    return ReturnKernelError(
        status,
        "kernel_read_first_changed_markdown_note_content",
        out_error);
  }

  return CopyKernelOwnedText(buffer, out_text, out_error);
}


int32_t sealed_kernel_bridge_get_search_note_default_limit(
    uint64_t* out_limit,
    char** out_error) {
  return KernelDefaultLimit(
      kernel_get_search_note_default_limit,
      "kernel_get_search_note_default_limit",
      out_limit,
      out_error);
}

int32_t sealed_kernel_bridge_get_backlink_default_limit(
    uint64_t* out_limit,
    char** out_error) {
  return KernelDefaultLimit(
      kernel_get_backlink_default_limit,
      "kernel_get_backlink_default_limit",
      out_limit,
      out_error);
}

int32_t sealed_kernel_bridge_get_tag_catalog_default_limit(
    uint64_t* out_limit,
    char** out_error) {
  return KernelDefaultLimit(
      kernel_get_tag_catalog_default_limit,
      "kernel_get_tag_catalog_default_limit",
      out_limit,
      out_error);
}

int32_t sealed_kernel_bridge_get_tag_note_default_limit(
    uint64_t* out_limit,
    char** out_error) {
  return KernelDefaultLimit(
      kernel_get_tag_note_default_limit,
      "kernel_get_tag_note_default_limit",
      out_limit,
      out_error);
}

int32_t sealed_kernel_bridge_get_tag_tree_default_limit(
    uint64_t* out_limit,
    char** out_error) {
  return KernelDefaultLimit(
      kernel_get_tag_tree_default_limit,
      "kernel_get_tag_tree_default_limit",
      out_limit,
      out_error);
}

int32_t sealed_kernel_bridge_get_graph_default_limit(
    uint64_t* out_limit,
    char** out_error) {
  return KernelDefaultLimit(
      kernel_get_graph_default_limit,
      "kernel_get_graph_default_limit",
      out_limit,
      out_error);
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

int32_t sealed_kernel_bridge_query_tag_tree_json(
    sealed_kernel_bridge_session* session,
    uint64_t limit,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (session == nullptr || session->handle == nullptr || out_json == nullptr || limit == 0) {
    SetError(out_error, "sealed kernel session is not open or tag-tree arguments are invalid.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_tag_tree tree{};
  const kernel_status status =
      kernel_query_tag_tree(session->handle, static_cast<size_t>(limit), &tree);
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_query_tag_tree", out_error);
  }

  std::string json = "{\"nodes\":[";
  for (size_t index = 0; index < tree.count; ++index) {
    if (index != 0) {
      json += ",";
    }
    AppendTagTreeNodeJson(json, tree.nodes[index]);
  }
  json += "],\"count\":" + std::to_string(tree.count) + "}";

  kernel_free_tag_tree(&tree);
  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "failed to allocate tag tree JSON.");
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

int32_t sealed_kernel_bridge_query_enriched_graph_json(
    sealed_kernel_bridge_session* session,
    uint64_t limit,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (session == nullptr || session->handle == nullptr || out_json == nullptr || limit == 0) {
    SetError(
        out_error,
        "sealed kernel session is not open or enriched graph arguments are invalid.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_owned_buffer buffer{};
  const kernel_status status = kernel_query_enriched_graph_json(
      session->handle,
      static_cast<size_t>(limit),
      &buffer);
  if (status.code != KERNEL_OK) {
    kernel_free_buffer(&buffer);
    return ReturnKernelError(status, "kernel_query_enriched_graph_json", out_error);
  }

  return CopyKernelOwnedText(buffer, out_json, out_error);
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


int32_t sealed_kernel_bridge_normalize_vault_relative_path_text(
    const char* rel_path_utf8,
    uint64_t rel_path_size,
    char** out_text,
    char** out_error) {
  if (out_text != nullptr) {
    *out_text = nullptr;
  }
  if (out_text == nullptr || (rel_path_size > 0 && rel_path_utf8 == nullptr)) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_owned_buffer buffer{};
  const kernel_status status = kernel_normalize_vault_relative_path(
      rel_path_utf8,
      static_cast<size_t>(rel_path_size),
      &buffer);
  if (status.code != KERNEL_OK) {
    kernel_free_buffer(&buffer);
    return ReturnKernelError(status, "kernel_normalize_vault_relative_path", out_error);
  }

  return CopyKernelOwnedText(buffer, out_text, out_error);
}


int32_t sealed_kernel_bridge_relativize_vault_path_text(
    sealed_kernel_bridge_session* session,
    const char* host_path_utf8,
    uint64_t host_path_size,
    uint8_t allow_empty,
    char** out_text,
    char** out_error) {
  if (out_text != nullptr) {
    *out_text = nullptr;
  }
  if (
      out_text == nullptr || session == nullptr || session->handle == nullptr ||
      (host_path_size > 0 && host_path_utf8 == nullptr)) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string host_path =
      Utf8ToActiveCodePage(host_path_utf8, host_path_size);
  if (host_path.empty()) {
    SetError(out_error, "host_path must be a non-empty UTF-8 string.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_owned_buffer buffer{};
  const kernel_status status = kernel_relativize_vault_path(
      session->handle,
      host_path.data(),
      host_path.size(),
      allow_empty,
      &buffer);
  if (status.code != KERNEL_OK) {
    kernel_free_buffer(&buffer);
    return ReturnKernelError(status, "kernel_relativize_vault_path", out_error);
  }

  const std::string active_text = buffer.data == nullptr || buffer.size == 0
                                      ? std::string()
                                      : std::string(buffer.data, buffer.size);
  kernel_free_buffer(&buffer);
  const std::string utf8_text =
      active_text.empty() ? std::string() : ActiveCodePageToUtf8(active_text.c_str());
  *out_text = CopyString(utf8_text);
  if (*out_text == nullptr) {
    SetError(out_error, "allocation_failed");
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

