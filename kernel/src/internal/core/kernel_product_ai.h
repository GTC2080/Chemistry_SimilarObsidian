// Reason: Keep AI/RAG product compute rules out of the public ABI wrapper.

#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace kernel::core::product {

std::size_t embedding_text_char_limit();
std::size_t rag_context_per_note_char_limit();
std::size_t ai_chat_timeout_secs();
std::size_t ai_ponder_timeout_secs();
std::size_t ai_embedding_request_timeout_secs();
std::size_t ai_embedding_cache_limit();
std::size_t ai_embedding_concurrency_limit();
std::size_t ai_rag_top_note_limit();
float ai_ponder_temperature();

std::string_view ai_ponder_system_prompt();
std::string_view derive_note_display_name_from_path(std::string_view note_path);
std::string normalize_ai_embedding_text(std::string_view text);
bool is_ai_embedding_text_indexable(std::string_view text);
std::string compute_ai_embedding_cache_key(
    std::string_view base_url,
    std::string_view model,
    std::string_view text);
std::string build_ai_rag_context(
    const char* const* note_names,
    const std::size_t* note_name_sizes,
    const char* const* note_contents,
    const std::size_t* note_content_sizes,
    std::size_t note_count);
std::string build_ai_rag_context_from_note_paths(
    const char* const* note_paths,
    const std::size_t* note_path_sizes,
    const char* const* note_contents,
    const std::size_t* note_content_sizes,
    std::size_t note_count);
std::string build_ai_rag_system_content(std::string_view context);
std::string build_ai_ponder_user_prompt(
    std::string_view topic,
    std::string_view context);

}  // namespace kernel::core::product
