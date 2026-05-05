// Reason: Expose product compute rules through the kernel C ABI so hosts only
// marshal DTOs and keep localized presentation text outside the kernel.

#include "kernel/c_api.h"

#include "core/kernel_shared.h"
#include "core/kernel_product_ai.h"
#include "core/kernel_product_context.h"
#include "core/kernel_product_database.h"
#include "core/kernel_product_paper.h"
#include "core/kernel_product_pubchem.h"
#include "core/kernel_product_study.h"
#include "core/kernel_product_truth.h"

#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <string>
#include <string_view>

namespace {

bool fill_owned_buffer(std::string_view value, kernel_owned_buffer* out_buffer) {
  out_buffer->data = nullptr;
  out_buffer->size = 0;
  if (value.empty()) {
    return true;
  }

  auto* owned = new (std::nothrow) char[value.size()];
  if (owned == nullptr) {
    return false;
  }
  std::memcpy(owned, value.data(), value.size());
  out_buffer->data = owned;
  out_buffer->size = value.size();
  return true;
}

std::uint32_t float_to_u32(const float value) {
  std::uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

float u32_to_float(const std::uint32_t bits) {
  float value = 0.0F;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

void write_u32_le(const std::uint32_t value, char* out) {
  out[0] = static_cast<char>(value & 0xFFu);
  out[1] = static_cast<char>((value >> 8u) & 0xFFu);
  out[2] = static_cast<char>((value >> 16u) & 0xFFu);
  out[3] = static_cast<char>((value >> 24u) & 0xFFu);
}

std::uint32_t read_u32_le(const std::uint8_t* in) {
  return static_cast<std::uint32_t>(in[0]) |
         (static_cast<std::uint32_t>(in[1]) << 8u) |
         (static_cast<std::uint32_t>(in[2]) << 16u) |
         (static_cast<std::uint32_t>(in[3]) << 24u);
}

kernel_status write_size_limit(std::size_t value, std::size_t* out_value) {
  if (out_value == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  *out_value = value;
  return kernel::core::make_status(KERNEL_OK);
}

kernel_status write_float_value(float value, float* out_value) {
  if (out_value == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  *out_value = value;
  return kernel::core::make_status(KERNEL_OK);
}

}  // namespace

extern "C" kernel_status kernel_compute_truth_diff(
    const char* prev_content,
    const std::size_t prev_size,
    const char* curr_content,
    const std::size_t curr_size,
    const char* file_extension,
    kernel_truth_diff_result* out_result) {
  if (out_result == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  if (
      (prev_size > 0 && prev_content == nullptr) ||
      (curr_size > 0 && curr_content == nullptr) || file_extension == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string_view prev(prev_content == nullptr ? "" : prev_content, prev_size);
  const std::string_view curr(curr_content == nullptr ? "" : curr_content, curr_size);
  return kernel::core::product::compute_truth_diff(prev, curr, file_extension, out_result);
}

extern "C" void kernel_free_truth_diff_result(kernel_truth_diff_result* result) {
  kernel::core::product::free_truth_diff_result(result);
}

extern "C" kernel_status kernel_get_truth_award_reason_key(
    const kernel_truth_award_reason reason,
    const char** out_key) {
  if (out_key == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  *out_key = nullptr;

  const char* key = kernel::core::product::truth_award_reason_key(reason);
  if (key == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  *out_key = key;
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_build_semantic_context(
    const char* content,
    const std::size_t content_size,
    kernel_owned_buffer* out_buffer) {
  if (out_buffer == nullptr || (content_size > 0 && content == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  const std::string_view raw(content == nullptr ? "" : content, content_size);
  const std::string context = kernel::core::product::build_semantic_context(raw);
  if (!fill_owned_buffer(context, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_get_semantic_context_min_bytes(std::size_t* out_bytes) {
  return write_size_limit(kernel::core::product::semantic_context_min_bytes(), out_bytes);
}

extern "C" kernel_status kernel_get_rag_context_per_note_char_limit(std::size_t* out_chars) {
  return write_size_limit(kernel::core::product::rag_context_per_note_char_limit(), out_chars);
}

extern "C" kernel_status kernel_get_embedding_text_char_limit(std::size_t* out_chars) {
  return write_size_limit(kernel::core::product::embedding_text_char_limit(), out_chars);
}

extern "C" kernel_status kernel_get_ai_chat_timeout_secs(std::size_t* out_secs) {
  return write_size_limit(kernel::core::product::ai_chat_timeout_secs(), out_secs);
}

extern "C" kernel_status kernel_get_ai_ponder_timeout_secs(std::size_t* out_secs) {
  return write_size_limit(kernel::core::product::ai_ponder_timeout_secs(), out_secs);
}

extern "C" kernel_status kernel_get_ai_embedding_request_timeout_secs(std::size_t* out_secs) {
  return write_size_limit(kernel::core::product::ai_embedding_request_timeout_secs(), out_secs);
}

extern "C" kernel_status kernel_get_ai_embedding_cache_limit(std::size_t* out_limit) {
  return write_size_limit(kernel::core::product::ai_embedding_cache_limit(), out_limit);
}

extern "C" kernel_status kernel_get_ai_embedding_concurrency_limit(std::size_t* out_limit) {
  return write_size_limit(kernel::core::product::ai_embedding_concurrency_limit(), out_limit);
}

extern "C" kernel_status kernel_get_ai_rag_top_note_limit(std::size_t* out_limit) {
  return write_size_limit(kernel::core::product::ai_rag_top_note_limit(), out_limit);
}

extern "C" kernel_status kernel_derive_file_extension_from_path(
    const char* path,
    const std::size_t path_size,
    kernel_owned_buffer* out_buffer) {
  if (out_buffer == nullptr || (path_size > 0 && path == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  const std::string_view path_view(path == nullptr ? "" : path, path_size);
  const std::string extension =
      kernel::core::product::derive_file_extension_from_path(path_view);
  if (!fill_owned_buffer(extension, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_derive_note_display_name_from_path(
    const char* path,
    const std::size_t path_size,
    kernel_owned_buffer* out_buffer) {
  if (out_buffer == nullptr || (path_size > 0 && path == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  const std::string_view path_view(path == nullptr ? "" : path, path_size);
  const std::string display_name(kernel::core::product::derive_note_display_name_from_path(path_view));
  if (!fill_owned_buffer(display_name, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_normalize_database_column_type(
    const char* column_type,
    const std::size_t column_type_size,
    kernel_owned_buffer* out_buffer) {
  if (out_buffer == nullptr || (column_type_size > 0 && column_type == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  const std::string_view column_type_view(
      column_type == nullptr ? "" : column_type,
      column_type_size);
  const std::string normalized =
      kernel::core::product::normalize_database_column_type(column_type_view);
  if (!fill_owned_buffer(normalized, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_normalize_database_json(
    const char* json,
    const std::size_t json_size,
    kernel_owned_buffer* out_buffer) {
  if (out_buffer == nullptr || (json_size > 0 && json == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  const std::string_view raw(json == nullptr ? "" : json, json_size);
  std::string normalized;
  if (!kernel::core::product::normalize_database_json(raw, normalized)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  if (!fill_owned_buffer(normalized, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_build_paper_compile_plan_json(
    const char* workspace,
    const std::size_t workspace_size,
    const char* template_name,
    const std::size_t template_name_size,
    const char* const* image_paths,
    const std::size_t* image_path_sizes,
    const std::size_t image_path_count,
    const char* csl_path,
    const std::size_t csl_path_size,
    const char* bibliography_path,
    const std::size_t bibliography_path_size,
    const char* resource_separator,
    const std::size_t resource_separator_size,
    kernel_owned_buffer* out_buffer) {
  if (
      out_buffer == nullptr || (workspace_size > 0 && workspace == nullptr) ||
      (template_name_size > 0 && template_name == nullptr) ||
      (image_path_count > 0 && (image_paths == nullptr || image_path_sizes == nullptr)) ||
      (csl_path_size > 0 && csl_path == nullptr) ||
      (bibliography_path_size > 0 && bibliography_path == nullptr) ||
      (resource_separator_size > 0 && resource_separator == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  for (std::size_t index = 0; index < image_path_count; ++index) {
    if (image_path_sizes[index] > 0 && image_paths[index] == nullptr) {
      return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
    }
  }

  const std::string_view workspace_view(workspace == nullptr ? "" : workspace, workspace_size);
  const std::string_view template_view(
      template_name == nullptr ? "" : template_name,
      template_name_size);
  const std::string_view csl_view(csl_path == nullptr ? "" : csl_path, csl_path_size);
  const std::string_view bibliography_view(
      bibliography_path == nullptr ? "" : bibliography_path,
      bibliography_path_size);
  const std::string_view separator_view(
      resource_separator == nullptr ? "" : resource_separator,
      resource_separator_size);
  const std::string plan = kernel::core::product::build_paper_compile_plan_json(
      workspace_view,
      template_view,
      image_paths,
      image_path_sizes,
      image_path_count,
      csl_view,
      bibliography_view,
      separator_view);
  if (!fill_owned_buffer(plan, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_get_default_paper_template(kernel_owned_buffer* out_buffer) {
  if (out_buffer == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  if (!fill_owned_buffer(kernel::core::product::default_paper_template(), out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_summarize_paper_compile_log_json(
    const char* log,
    const std::size_t log_size,
    const std::size_t log_char_limit,
    kernel_owned_buffer* out_buffer) {
  if (out_buffer == nullptr || (log_size > 0 && log == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  const std::string_view raw(log == nullptr ? "" : log, log_size);
  const std::string summary =
      kernel::core::product::build_paper_compile_log_summary_json(raw, log_char_limit);
  if (!fill_owned_buffer(summary, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_normalize_pubchem_query(
    const char* query,
    const std::size_t query_size,
    kernel_owned_buffer* out_buffer) {
  if (out_buffer == nullptr || (query_size > 0 && query == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  const std::string_view raw(query == nullptr ? "" : query, query_size);
  const std::string normalized = kernel::core::product::normalize_pubchem_query(raw);
  if (normalized.empty()) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  if (!fill_owned_buffer(normalized, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_build_pubchem_compound_info_json(
    const char* query,
    const std::size_t query_size,
    const char* formula,
    const std::size_t formula_size,
    const double molecular_weight,
    const std::uint8_t has_density,
    const double density,
    const std::size_t property_count,
    kernel_owned_buffer* out_buffer) {
  if (
      out_buffer == nullptr || (query_size > 0 && query == nullptr) ||
      (formula_size > 0 && formula == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  const std::string_view query_view(query == nullptr ? "" : query, query_size);
  const std::string_view formula_view(formula == nullptr ? "" : formula, formula_size);
  const std::string payload = kernel::core::product::build_pubchem_compound_info_payload_json(
      query_view,
      formula_view,
      molecular_weight,
      has_density != 0,
      density,
      property_count);
  if (!fill_owned_buffer(payload, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_normalize_ai_embedding_text(
    const char* text,
    const std::size_t text_size,
    kernel_owned_buffer* out_buffer) {
  if (out_buffer == nullptr || (text_size > 0 && text == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  const std::string_view raw(text == nullptr ? "" : text, text_size);
  const std::string normalized = kernel::core::product::normalize_ai_embedding_text(raw);
  if (!kernel::core::product::is_ai_embedding_text_indexable(normalized)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  if (!fill_owned_buffer(normalized, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_is_ai_embedding_text_indexable(
    const char* text,
    const std::size_t text_size,
    std::uint8_t* out_is_indexable) {
  if (out_is_indexable == nullptr || (text_size > 0 && text == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string_view raw(text == nullptr ? "" : text, text_size);
  const std::string normalized = kernel::core::product::normalize_ai_embedding_text(raw);
  *out_is_indexable =
      static_cast<std::uint8_t>(kernel::core::product::is_ai_embedding_text_indexable(normalized));
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_should_refresh_ai_embedding_note(
    const std::int64_t note_updated_at,
    const std::uint8_t has_existing_updated_at,
    const std::int64_t existing_updated_at,
    std::uint8_t* out_should_refresh) {
  if (out_should_refresh == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  *out_should_refresh = static_cast<std::uint8_t>(
      has_existing_updated_at == 0 || note_updated_at > existing_updated_at);
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_compute_ai_embedding_cache_key(
    const char* base_url,
    const std::size_t base_url_size,
    const char* model,
    const std::size_t model_size,
    const char* text,
    const std::size_t text_size,
    kernel_owned_buffer* out_buffer) {
  if (
      out_buffer == nullptr || (base_url_size > 0 && base_url == nullptr) ||
      (model_size > 0 && model == nullptr) || (text_size > 0 && text == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  const std::string_view base_url_view(base_url == nullptr ? "" : base_url, base_url_size);
  const std::string_view model_view(model == nullptr ? "" : model, model_size);
  const std::string_view text_view(text == nullptr ? "" : text, text_size);
  const std::string key = kernel::core::product::compute_ai_embedding_cache_key(
      base_url_view,
      model_view,
      text_view);
  if (!fill_owned_buffer(key, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_serialize_ai_embedding_blob(
    const float* values,
    const std::size_t value_count,
    kernel_owned_buffer* out_buffer) {
  static_assert(sizeof(float) == 4, "AI embedding blob codec requires 32-bit floats");
  if (out_buffer == nullptr || (value_count > 0 && values == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  if (value_count > (std::numeric_limits<std::size_t>::max)() / 4u) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::string blob(value_count * 4u, '\0');
  for (std::size_t index = 0; index < value_count; ++index) {
    write_u32_le(float_to_u32(values[index]), blob.data() + (index * 4u));
  }
  if (!fill_owned_buffer(blob, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_parse_ai_embedding_blob(
    const std::uint8_t* blob,
    const std::size_t blob_size,
    kernel_float_buffer* out_values) {
  static_assert(sizeof(float) == 4, "AI embedding blob codec requires 32-bit floats");
  if (out_values == nullptr || (blob_size > 0 && blob == nullptr) || blob_size % 4u != 0u) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_values->values = nullptr;
  out_values->count = 0;

  const std::size_t value_count = blob_size / 4u;
  if (value_count == 0) {
    return kernel::core::make_status(KERNEL_OK);
  }

  auto* values = new (std::nothrow) float[value_count];
  if (values == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  for (std::size_t index = 0; index < value_count; ++index) {
    values[index] = u32_to_float(read_u32_le(blob + (index * 4u)));
  }

  out_values->values = values;
  out_values->count = value_count;
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" void kernel_free_float_buffer(kernel_float_buffer* buffer) {
  if (buffer == nullptr) {
    return;
  }
  delete[] buffer->values;
  buffer->values = nullptr;
  buffer->count = 0;
}

extern "C" kernel_status kernel_build_ai_rag_context(
    const char* const* note_names,
    const std::size_t* note_name_sizes,
    const char* const* note_contents,
    const std::size_t* note_content_sizes,
    const std::size_t note_count,
    kernel_owned_buffer* out_buffer) {
  if (
      out_buffer == nullptr ||
      (note_count > 0 &&
       (note_names == nullptr || note_name_sizes == nullptr || note_contents == nullptr ||
        note_content_sizes == nullptr))) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  for (std::size_t index = 0; index < note_count; ++index) {
    if (
        (note_name_sizes[index] > 0 && note_names[index] == nullptr) ||
        (note_content_sizes[index] > 0 && note_contents[index] == nullptr)) {
      return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
    }
  }

  const std::string context = kernel::core::product::build_ai_rag_context(
      note_names,
      note_name_sizes,
      note_contents,
      note_content_sizes,
      note_count);
  if (!fill_owned_buffer(context, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_build_ai_rag_context_from_note_paths(
    const char* const* note_paths,
    const std::size_t* note_path_sizes,
    const char* const* note_contents,
    const std::size_t* note_content_sizes,
    const std::size_t note_count,
    kernel_owned_buffer* out_buffer) {
  if (
      out_buffer == nullptr ||
      (note_count > 0 &&
       (note_paths == nullptr || note_path_sizes == nullptr || note_contents == nullptr ||
        note_content_sizes == nullptr))) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  for (std::size_t index = 0; index < note_count; ++index) {
    if (
        (note_path_sizes[index] > 0 && note_paths[index] == nullptr) ||
        (note_content_sizes[index] > 0 && note_contents[index] == nullptr)) {
      return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
    }
  }

  const std::string context = kernel::core::product::build_ai_rag_context_from_note_paths(
      note_paths,
      note_path_sizes,
      note_contents,
      note_content_sizes,
      note_count);
  if (!fill_owned_buffer(context, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_build_ai_rag_system_content(
    const char* context,
    const std::size_t context_size,
    kernel_owned_buffer* out_buffer) {
  if (out_buffer == nullptr || (context_size > 0 && context == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  const std::string_view context_view(context == nullptr ? "" : context, context_size);
  const std::string content = kernel::core::product::build_ai_rag_system_content(context_view);
  if (!fill_owned_buffer(content, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_get_ai_ponder_system_prompt(kernel_owned_buffer* out_buffer) {
  if (out_buffer == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  if (!fill_owned_buffer(kernel::core::product::ai_ponder_system_prompt(), out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_build_ai_ponder_user_prompt(
    const char* topic,
    const std::size_t topic_size,
    const char* context,
    const std::size_t context_size,
    kernel_owned_buffer* out_buffer) {
  if (
      out_buffer == nullptr || (topic_size > 0 && topic == nullptr) ||
      (context_size > 0 && context == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  const std::string_view topic_view(topic == nullptr ? "" : topic, topic_size);
  const std::string_view context_view(context == nullptr ? "" : context, context_size);
  const std::string prompt =
      kernel::core::product::build_ai_ponder_user_prompt(topic_view, context_view);
  if (!fill_owned_buffer(prompt, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_get_ai_ponder_temperature(float* out_temperature) {
  return write_float_value(kernel::core::product::ai_ponder_temperature(), out_temperature);
}

extern "C" kernel_status kernel_compute_truth_state_from_activity(
    const kernel_study_note_activity* activities,
    const std::size_t activity_count,
    kernel_truth_state_snapshot* out_state) {
  return kernel::core::product::compute_truth_state_from_activity(
      activities, activity_count, out_state);
}

extern "C" kernel_status kernel_compute_study_stats_window(
    const std::int64_t now_epoch_secs,
    const std::int64_t days_back,
    kernel_study_stats_window* out_window) {
  return kernel::core::product::compute_study_stats_window(
      now_epoch_secs, days_back, out_window);
}

extern "C" kernel_status kernel_compute_study_streak_days(
    const std::int64_t* day_buckets,
    const std::size_t day_count,
    const std::int64_t today_bucket,
    std::int64_t* out_streak_days) {
  return kernel::core::product::compute_study_streak_days(
      day_buckets, day_count, today_bucket, out_streak_days);
}

extern "C" kernel_status kernel_compute_study_streak_days_from_timestamps(
    const std::int64_t* started_at_epoch_secs,
    const std::size_t timestamp_count,
    const std::int64_t today_bucket,
    std::int64_t* out_streak_days) {
  return kernel::core::product::compute_study_streak_days_from_timestamps(
      started_at_epoch_secs, timestamp_count, today_bucket, out_streak_days);
}

extern "C" kernel_status kernel_build_study_heatmap_grid(
    const kernel_heatmap_day_activity* days,
    const std::size_t day_count,
    const std::int64_t now_epoch_secs,
    kernel_heatmap_grid* out_grid) {
  return kernel::core::product::build_study_heatmap_grid(
      days, day_count, now_epoch_secs, out_grid);
}

extern "C" void kernel_free_study_heatmap_grid(kernel_heatmap_grid* grid) {
  kernel::core::product::free_study_heatmap_grid(grid);
}
