#include "sealed_kernel_bridge_internal.h"

using namespace sealed_kernel_bridge_internal;

int32_t sealed_kernel_bridge_get_semantic_context_min_bytes(
    uint64_t* out_bytes,
    char** out_error) {
  return KernelDefaultLimit(
      kernel_get_semantic_context_min_bytes,
      "kernel_get_semantic_context_min_bytes",
      out_bytes,
      out_error);
}

int32_t sealed_kernel_bridge_get_ai_chat_timeout_secs(
    uint64_t* out_secs,
    char** out_error) {
  return KernelDefaultLimit(
      kernel_get_ai_chat_timeout_secs,
      "kernel_get_ai_chat_timeout_secs",
      out_secs,
      out_error);
}

int32_t sealed_kernel_bridge_get_ai_ponder_timeout_secs(
    uint64_t* out_secs,
    char** out_error) {
  return KernelDefaultLimit(
      kernel_get_ai_ponder_timeout_secs,
      "kernel_get_ai_ponder_timeout_secs",
      out_secs,
      out_error);
}

int32_t sealed_kernel_bridge_get_ai_embedding_request_timeout_secs(
    uint64_t* out_secs,
    char** out_error) {
  return KernelDefaultLimit(
      kernel_get_ai_embedding_request_timeout_secs,
      "kernel_get_ai_embedding_request_timeout_secs",
      out_secs,
      out_error);
}

int32_t sealed_kernel_bridge_get_ai_embedding_cache_limit(
    uint64_t* out_limit,
    char** out_error) {
  return KernelDefaultLimit(
      kernel_get_ai_embedding_cache_limit,
      "kernel_get_ai_embedding_cache_limit",
      out_limit,
      out_error);
}

int32_t sealed_kernel_bridge_get_ai_embedding_concurrency_limit(
    uint64_t* out_limit,
    char** out_error) {
  return KernelDefaultLimit(
      kernel_get_ai_embedding_concurrency_limit,
      "kernel_get_ai_embedding_concurrency_limit",
      out_limit,
      out_error);
}

int32_t sealed_kernel_bridge_get_ai_rag_top_note_limit(
    uint64_t* out_limit,
    char** out_error) {
  return KernelDefaultLimit(
      kernel_get_ai_rag_top_note_limit,
      "kernel_get_ai_rag_top_note_limit",
      out_limit,
      out_error);
}

int32_t sealed_kernel_bridge_normalize_ai_embedding_text(
    const char* text_utf8,
    uint64_t text_size,
    char** out_text,
    char** out_error) {
  if (out_text != nullptr) {
    *out_text = nullptr;
  }
  if (out_text == nullptr || (text_size > 0 && text_utf8 == nullptr)) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_owned_buffer buffer{};
  const kernel_status status = kernel_normalize_ai_embedding_text(
      text_utf8,
      static_cast<size_t>(text_size),
      &buffer);
  if (status.code != KERNEL_OK) {
    kernel_free_buffer(&buffer);
    SetError(
        out_error,
        status.code == KERNEL_ERROR_INVALID_ARGUMENT
            ? "empty_text"
            : "normalize_embedding_text_failed");
    return static_cast<int32_t>(status.code);
  }

  return CopyKernelOwnedText(buffer, out_text, out_error);
}


int32_t sealed_kernel_bridge_compute_ai_embedding_cache_key(
    const char* base_url_utf8,
    uint64_t base_url_size,
    const char* model_utf8,
    uint64_t model_size,
    const char* text_utf8,
    uint64_t text_size,
    char** out_key,
    char** out_error) {
  if (out_key != nullptr) {
    *out_key = nullptr;
  }
  if (
      out_key == nullptr || (base_url_size > 0 && base_url_utf8 == nullptr) ||
      (model_size > 0 && model_utf8 == nullptr) ||
      (text_size > 0 && text_utf8 == nullptr)) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_owned_buffer buffer{};
  const kernel_status status = kernel_compute_ai_embedding_cache_key(
      base_url_utf8,
      static_cast<size_t>(base_url_size),
      model_utf8,
      static_cast<size_t>(model_size),
      text_utf8,
      static_cast<size_t>(text_size),
      &buffer);
  if (status.code != KERNEL_OK) {
    kernel_free_buffer(&buffer);
    return ReturnKernelError(status, "kernel_compute_ai_embedding_cache_key", out_error);
  }

  return CopyKernelOwnedText(buffer, out_key, out_error);
}

int32_t sealed_kernel_bridge_serialize_ai_embedding_blob(
    const float* values,
    uint64_t value_count,
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
      out_bytes == nullptr || out_size == nullptr ||
      (value_count > 0 && values == nullptr) ||
      value_count > (std::numeric_limits<std::size_t>::max)()) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_owned_buffer buffer{};
  const kernel_status status = kernel_serialize_ai_embedding_blob(
      values,
      static_cast<std::size_t>(value_count),
      &buffer);
  if (status.code != KERNEL_OK) {
    kernel_free_buffer(&buffer);
    SetError(out_error, "embedding_blob_serialize_failed");
    return static_cast<int32_t>(status.code);
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

int32_t sealed_kernel_bridge_parse_ai_embedding_blob(
    const uint8_t* blob,
    uint64_t blob_size,
    float** out_values,
    uint64_t* out_count,
    char** out_error) {
  if (out_values != nullptr) {
    *out_values = nullptr;
  }
  if (out_count != nullptr) {
    *out_count = 0;
  }
  if (
      out_values == nullptr || out_count == nullptr ||
      (blob_size > 0 && blob == nullptr) ||
      blob_size > (std::numeric_limits<std::size_t>::max)()) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_float_buffer parsed{};
  const kernel_status status = kernel_parse_ai_embedding_blob(
      blob,
      static_cast<std::size_t>(blob_size),
      &parsed);
  if (status.code != KERNEL_OK) {
    kernel_free_float_buffer(&parsed);
    SetError(out_error, "embedding_blob_parse_failed");
    return static_cast<int32_t>(status.code);
  }

  auto* copied = CopyFloatArray(parsed.values, parsed.count);
  const std::size_t copied_count = parsed.count;
  kernel_free_float_buffer(&parsed);
  if (copied == nullptr && copied_count != 0) {
    SetError(out_error, "allocation_failed");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }

  *out_values = copied;
  *out_count = static_cast<uint64_t>(copied_count);
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_prepare_ai_embedding_refresh_jobs_json(
    sealed_kernel_bridge_session* session,
    const char* ignored_roots_utf8,
    uint64_t limit,
    uint8_t force_refresh,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (session == nullptr || session->handle == nullptr || out_json == nullptr || limit == 0 ||
      limit > (std::numeric_limits<std::size_t>::max)()) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string ignored_roots = Utf8ToActiveCodePage(ignored_roots_utf8);
  kernel_ai_embedding_refresh_job_list jobs{};
  const kernel_status status = kernel_prepare_ai_embedding_refresh_jobs(
      session->handle,
      ignored_roots.empty() ? nullptr : ignored_roots.c_str(),
      static_cast<size_t>(limit),
      force_refresh,
      &jobs);
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_prepare_ai_embedding_refresh_jobs", out_error);
  }

  std::string json = "{\"jobs\":[";
  for (size_t index = 0; index < jobs.count; ++index) {
    if (index != 0) {
      json += ",";
    }
    AppendAiEmbeddingRefreshJobJson(json, jobs.jobs[index]);
  }
  json += "],\"count\":" + std::to_string(jobs.count) + "}";

  kernel_free_ai_embedding_refresh_job_list(&jobs);
  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "allocation_failed");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_prepare_changed_ai_embedding_refresh_jobs_json(
    sealed_kernel_bridge_session* session,
    const char* changed_paths_lf_utf8,
    uint64_t limit,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (session == nullptr || session->handle == nullptr || out_json == nullptr || limit == 0 ||
      limit > (std::numeric_limits<std::size_t>::max)()) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string changed_paths = Utf8ToActiveCodePage(changed_paths_lf_utf8);
  kernel_ai_embedding_refresh_job_list jobs{};
  const kernel_status status = kernel_prepare_changed_ai_embedding_refresh_jobs(
      session->handle,
      changed_paths.c_str(),
      static_cast<size_t>(limit),
      &jobs);
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(
        status,
        "kernel_prepare_changed_ai_embedding_refresh_jobs",
        out_error);
  }

  std::string json = "{\"jobs\":[";
  for (size_t index = 0; index < jobs.count; ++index) {
    if (index != 0) {
      json += ",";
    }
    AppendAiEmbeddingRefreshJobJson(json, jobs.jobs[index]);
  }
  json += "],\"count\":" + std::to_string(jobs.count) + "}";

  kernel_free_ai_embedding_refresh_job_list(&jobs);
  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "allocation_failed");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_update_ai_embedding(
    sealed_kernel_bridge_session* session,
    const char* rel_path_utf8,
    const float* values,
    uint64_t value_count,
    char** out_error) {
  if (session == nullptr || session->handle == nullptr || rel_path_utf8 == nullptr ||
      values == nullptr || value_count == 0 ||
      value_count > (std::numeric_limits<std::size_t>::max)()) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string rel_path = Utf8ToActiveCodePage(rel_path_utf8);
  const kernel_status status =
      kernel_update_ai_embedding(
          session->handle,
          rel_path.c_str(),
          values,
          static_cast<size_t>(value_count));
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_update_ai_embedding", out_error);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_clear_ai_embeddings(
    sealed_kernel_bridge_session* session,
    char** out_error) {
  if (session == nullptr || session->handle == nullptr) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const kernel_status status = kernel_clear_ai_embeddings(session->handle);
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_clear_ai_embeddings", out_error);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_delete_changed_ai_embedding_notes(
    sealed_kernel_bridge_session* session,
    const char* changed_paths_lf_utf8,
    uint64_t* out_deleted_count,
    char** out_error) {
  if (out_deleted_count != nullptr) {
    *out_deleted_count = 0;
  }
  if (session == nullptr || session->handle == nullptr || out_deleted_count == nullptr) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string changed_paths = Utf8ToActiveCodePage(changed_paths_lf_utf8);
  const kernel_status status = kernel_delete_changed_ai_embedding_notes(
      session->handle,
      changed_paths.c_str(),
      out_deleted_count);
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_delete_changed_ai_embedding_notes", out_error);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_query_ai_embedding_top_notes_json(
    sealed_kernel_bridge_session* session,
    const float* query_values,
    uint64_t query_value_count,
    const char* exclude_rel_path_utf8,
    uint64_t limit,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (session == nullptr || session->handle == nullptr || out_json == nullptr ||
      query_values == nullptr || query_value_count == 0 || limit == 0 ||
      query_value_count > (std::numeric_limits<std::size_t>::max)() ||
      limit > (std::numeric_limits<std::size_t>::max)()) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string exclude_rel_path = Utf8ToActiveCodePage(exclude_rel_path_utf8);
  kernel_search_results results{};
  const kernel_status status =
      kernel_query_ai_embedding_top_notes(
          session->handle,
          query_values,
          static_cast<size_t>(query_value_count),
          exclude_rel_path.empty() ? nullptr : exclude_rel_path.c_str(),
          static_cast<size_t>(limit),
          &results);
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_query_ai_embedding_top_notes", out_error);
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
    SetError(out_error, "allocation_failed");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_build_ai_rag_system_content_text(
    const char* context_utf8,
    uint64_t context_size,
    char** out_text,
    char** out_error) {
  if (out_text != nullptr) {
    *out_text = nullptr;
  }
  if (out_text == nullptr || (context_size > 0 && context_utf8 == nullptr)) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_owned_buffer buffer{};
  const kernel_status status = kernel_build_ai_rag_system_content(
      context_utf8,
      static_cast<size_t>(context_size),
      &buffer);
  if (status.code != KERNEL_OK) {
    kernel_free_buffer(&buffer);
    return ReturnKernelError(status, "kernel_build_ai_rag_system_content", out_error);
  }

  return CopyKernelOwnedText(buffer, out_text, out_error);
}

int32_t sealed_kernel_bridge_build_ai_rag_context_from_note_paths_text(
    const char* const* note_paths_utf8,
    const uint64_t* note_path_sizes,
    const char* const* note_contents_utf8,
    const uint64_t* note_content_sizes,
    uint64_t note_count,
    char** out_text,
    char** out_error) {
  if (out_text != nullptr) {
    *out_text = nullptr;
  }
  if (
      out_text == nullptr ||
      (note_count > 0 &&
       (note_paths_utf8 == nullptr || note_path_sizes == nullptr ||
        note_contents_utf8 == nullptr || note_content_sizes == nullptr))) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::vector<size_t> path_sizes;
  std::vector<size_t> content_sizes;
  path_sizes.reserve(static_cast<size_t>(note_count));
  content_sizes.reserve(static_cast<size_t>(note_count));
  for (uint64_t index = 0; index < note_count; ++index) {
    if (
        (note_path_sizes[index] > 0 && note_paths_utf8[index] == nullptr) ||
        (note_content_sizes[index] > 0 && note_contents_utf8[index] == nullptr)) {
      SetError(out_error, "invalid_argument");
      return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
    }
    path_sizes.push_back(static_cast<size_t>(note_path_sizes[index]));
    content_sizes.push_back(static_cast<size_t>(note_content_sizes[index]));
  }

  kernel_owned_buffer buffer{};
  const kernel_status status = kernel_build_ai_rag_context_from_note_paths(
      note_paths_utf8,
      path_sizes.data(),
      note_contents_utf8,
      content_sizes.data(),
      static_cast<size_t>(note_count),
      &buffer);
  if (status.code != KERNEL_OK) {
    kernel_free_buffer(&buffer);
    return ReturnKernelError(status, "kernel_build_ai_rag_context_from_note_paths", out_error);
  }

  return CopyKernelOwnedText(buffer, out_text, out_error);
}

int32_t sealed_kernel_bridge_build_ai_rag_context_from_changed_note_paths_text(
    sealed_kernel_bridge_session* session,
    const char* note_paths_lf_utf8,
    char** out_text,
    char** out_error) {
  if (out_text != nullptr) {
    *out_text = nullptr;
  }
  if (session == nullptr || session->handle == nullptr || out_text == nullptr) {
    SetError(out_error, "sealed kernel session is not open.");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string note_paths = Utf8ToActiveCodePage(note_paths_lf_utf8);
  kernel_owned_buffer buffer{};
  const kernel_status status = kernel_build_ai_rag_context_from_changed_note_paths(
      session->handle,
      note_paths.c_str(),
      &buffer);
  if (status.code != KERNEL_OK) {
    kernel_free_buffer(&buffer);
    return ReturnKernelError(
        status,
        "kernel_build_ai_rag_context_from_changed_note_paths",
        out_error);
  }

  return CopyKernelOwnedText(buffer, out_text, out_error);
}

int32_t sealed_kernel_bridge_get_ai_ponder_system_prompt_text(
    char** out_text,
    char** out_error) {
  if (out_text != nullptr) {
    *out_text = nullptr;
  }
  if (out_text == nullptr) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_owned_buffer buffer{};
  const kernel_status status = kernel_get_ai_ponder_system_prompt(&buffer);
  if (status.code != KERNEL_OK) {
    kernel_free_buffer(&buffer);
    return ReturnKernelError(status, "kernel_get_ai_ponder_system_prompt", out_error);
  }

  return CopyKernelOwnedText(buffer, out_text, out_error);
}

int32_t sealed_kernel_bridge_build_ai_ponder_user_prompt_text(
    const char* topic_utf8,
    uint64_t topic_size,
    const char* context_utf8,
    uint64_t context_size,
    char** out_text,
    char** out_error) {
  if (out_text != nullptr) {
    *out_text = nullptr;
  }
  if (
      out_text == nullptr || (topic_size > 0 && topic_utf8 == nullptr) ||
      (context_size > 0 && context_utf8 == nullptr)) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_owned_buffer buffer{};
  const kernel_status status = kernel_build_ai_ponder_user_prompt(
      topic_utf8,
      static_cast<size_t>(topic_size),
      context_utf8,
      static_cast<size_t>(context_size),
      &buffer);
  if (status.code != KERNEL_OK) {
    kernel_free_buffer(&buffer);
    return ReturnKernelError(status, "kernel_build_ai_ponder_user_prompt", out_error);
  }

  return CopyKernelOwnedText(buffer, out_text, out_error);
}

int32_t sealed_kernel_bridge_get_ai_ponder_temperature(
    float* out_temperature,
    char** out_error) {
  return KernelDefaultFloat(
      kernel_get_ai_ponder_temperature,
      "kernel_get_ai_ponder_temperature",
      out_temperature,
      out_error);
}


