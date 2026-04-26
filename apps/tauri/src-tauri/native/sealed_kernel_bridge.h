#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sealed_kernel_bridge_session sealed_kernel_bridge_session;

typedef struct sealed_kernel_bridge_state_snapshot {
  int32_t session_state;
  int32_t index_state;
  uint64_t indexed_note_count;
  uint64_t pending_recovery_ops;
} sealed_kernel_bridge_state_snapshot;

char* sealed_kernel_bridge_info_json(void);
void sealed_kernel_bridge_free_string(char* value);

int32_t sealed_kernel_bridge_open_vault_utf8(
    const char* vault_path_utf8,
    sealed_kernel_bridge_session** out_session,
    char** out_error);

void sealed_kernel_bridge_close(sealed_kernel_bridge_session* session);

int32_t sealed_kernel_bridge_get_state(
    sealed_kernel_bridge_session* session,
    sealed_kernel_bridge_state_snapshot* out_state,
    char** out_error);

int32_t sealed_kernel_bridge_query_notes_json(
    sealed_kernel_bridge_session* session,
    uint64_t limit,
    char** out_json,
    char** out_error);

int32_t sealed_kernel_bridge_query_notes_filtered_json(
    sealed_kernel_bridge_session* session,
    uint64_t limit,
    const char* ignored_roots_utf8,
    char** out_json,
    char** out_error);

int32_t sealed_kernel_bridge_query_file_tree_json(
    sealed_kernel_bridge_session* session,
    uint64_t limit,
    const char* ignored_roots_utf8,
    char** out_json,
    char** out_error);

int32_t sealed_kernel_bridge_filter_changed_markdown_paths_json(
    const char* changed_paths_lf_utf8,
    char** out_json,
    char** out_error);

int32_t sealed_kernel_bridge_filter_supported_vault_paths_json(
    const char* changed_paths_lf_utf8,
    char** out_json,
    char** out_error);

int32_t sealed_kernel_bridge_read_note_json(
    sealed_kernel_bridge_session* session,
    const char* rel_path_utf8,
    char** out_json,
    char** out_error);

int32_t sealed_kernel_bridge_write_note_json(
    sealed_kernel_bridge_session* session,
    const char* rel_path_utf8,
    const char* content_utf8,
    uint64_t content_size,
    char** out_json,
    char** out_error);

int32_t sealed_kernel_bridge_query_search_notes_json(
    sealed_kernel_bridge_session* session,
    const char* query_utf8,
    uint64_t limit,
    char** out_json,
    char** out_error);

int32_t sealed_kernel_bridge_query_tags_json(
    sealed_kernel_bridge_session* session,
    uint64_t limit,
    char** out_json,
    char** out_error);

int32_t sealed_kernel_bridge_query_tag_notes_json(
    sealed_kernel_bridge_session* session,
    const char* tag_utf8,
    uint64_t limit,
    char** out_json,
    char** out_error);

int32_t sealed_kernel_bridge_query_graph_json(
    sealed_kernel_bridge_session* session,
    uint64_t limit,
    char** out_json,
    char** out_error);

int32_t sealed_kernel_bridge_query_backlinks_json(
    sealed_kernel_bridge_session* session,
    const char* rel_path_utf8,
    uint64_t limit,
    char** out_json,
    char** out_error);

int32_t sealed_kernel_bridge_query_chem_spectra_json(
    sealed_kernel_bridge_session* session,
    uint64_t limit,
    char** out_json,
    char** out_error);

int32_t sealed_kernel_bridge_get_chem_spectrum_json(
    sealed_kernel_bridge_session* session,
    const char* attachment_rel_path_utf8,
    char** out_json,
    char** out_error);

int32_t sealed_kernel_bridge_query_note_chem_spectrum_refs_json(
    sealed_kernel_bridge_session* session,
    const char* note_rel_path_utf8,
    uint64_t limit,
    char** out_json,
    char** out_error);

int32_t sealed_kernel_bridge_query_chem_spectrum_referrers_json(
    sealed_kernel_bridge_session* session,
    const char* attachment_rel_path_utf8,
    uint64_t limit,
    char** out_json,
    char** out_error);

int32_t sealed_kernel_bridge_create_folder(
    sealed_kernel_bridge_session* session,
    const char* folder_rel_path_utf8,
    char** out_error);

int32_t sealed_kernel_bridge_delete_entry(
    sealed_kernel_bridge_session* session,
    const char* target_rel_path_utf8,
    char** out_error);

int32_t sealed_kernel_bridge_rename_entry(
    sealed_kernel_bridge_session* session,
    const char* source_rel_path_utf8,
    const char* new_name_utf8,
    char** out_error);

int32_t sealed_kernel_bridge_move_entry(
    sealed_kernel_bridge_session* session,
    const char* source_rel_path_utf8,
    const char* dest_folder_rel_path_utf8,
    char** out_error);

#ifdef __cplusplus
}
#endif
