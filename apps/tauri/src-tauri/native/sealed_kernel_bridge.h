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
void sealed_kernel_bridge_free_bytes(uint8_t* value);
void sealed_kernel_bridge_free_float_array(float* value);

int32_t sealed_kernel_bridge_open_vault_utf8(
    const char* vault_path_utf8,
    sealed_kernel_bridge_session** out_session,
    char** out_error);

void sealed_kernel_bridge_close(sealed_kernel_bridge_session* session);

int32_t sealed_kernel_bridge_get_state(
    sealed_kernel_bridge_session* session,
    sealed_kernel_bridge_state_snapshot* out_state,
    char** out_error);

int32_t sealed_kernel_bridge_get_note_catalog_default_limit(
    uint64_t* out_limit,
    char** out_error);

int32_t sealed_kernel_bridge_get_note_query_default_limit(
    uint64_t* out_limit,
    char** out_error);

int32_t sealed_kernel_bridge_get_vault_scan_default_limit(
    uint64_t* out_limit,
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

int32_t sealed_kernel_bridge_get_file_tree_default_limit(
    uint64_t* out_limit,
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

int32_t sealed_kernel_bridge_filter_supported_vault_paths_filtered_json(
    const char* changed_paths_lf_utf8,
    const char* ignored_roots_utf8,
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

int32_t sealed_kernel_bridge_get_search_note_default_limit(
    uint64_t* out_limit,
    char** out_error);

int32_t sealed_kernel_bridge_get_backlink_default_limit(
    uint64_t* out_limit,
    char** out_error);

int32_t sealed_kernel_bridge_get_tag_catalog_default_limit(
    uint64_t* out_limit,
    char** out_error);

int32_t sealed_kernel_bridge_get_tag_note_default_limit(
    uint64_t* out_limit,
    char** out_error);

int32_t sealed_kernel_bridge_get_tag_tree_default_limit(
    uint64_t* out_limit,
    char** out_error);

int32_t sealed_kernel_bridge_get_graph_default_limit(
    uint64_t* out_limit,
    char** out_error);

int32_t sealed_kernel_bridge_query_tags_json(
    sealed_kernel_bridge_session* session,
    uint64_t limit,
    char** out_json,
    char** out_error);

int32_t sealed_kernel_bridge_query_tag_tree_json(
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

int32_t sealed_kernel_bridge_get_chem_spectra_default_limit(
    uint64_t* out_limit,
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

int32_t sealed_kernel_bridge_get_note_chem_spectrum_refs_default_limit(
    uint64_t* out_limit,
    char** out_error);

int32_t sealed_kernel_bridge_query_chem_spectrum_referrers_json(
    sealed_kernel_bridge_session* session,
    const char* attachment_rel_path_utf8,
    uint64_t limit,
    char** out_json,
    char** out_error);

int32_t sealed_kernel_bridge_get_chem_spectrum_referrers_default_limit(
    uint64_t* out_limit,
    char** out_error);

int32_t sealed_kernel_bridge_parse_spectroscopy_text_json(
    const char* raw_utf8,
    uint64_t raw_size,
    const char* extension_utf8,
    char** out_json,
    char** out_error);

int32_t sealed_kernel_bridge_compute_truth_diff_json(
    const char* prev_content,
    uint64_t prev_size,
    const char* curr_content,
    uint64_t curr_size,
    const char* file_extension_utf8,
    char** out_json,
    char** out_error);

int32_t sealed_kernel_bridge_build_semantic_context_text(
    const char* content,
    uint64_t content_size,
    char** out_text,
    char** out_error);

int32_t sealed_kernel_bridge_derive_file_extension_from_path_text(
    const char* path_utf8,
    uint64_t path_size,
    char** out_text,
    char** out_error);

int32_t sealed_kernel_bridge_normalize_vault_relative_path_text(
    const char* rel_path_utf8,
    uint64_t rel_path_size,
    char** out_text,
    char** out_error);

int32_t sealed_kernel_bridge_derive_note_display_name_from_path_text(
    const char* path_utf8,
    uint64_t path_size,
    char** out_text,
    char** out_error);

int32_t sealed_kernel_bridge_normalize_database_column_type_text(
    const char* column_type_utf8,
    uint64_t column_type_size,
    char** out_text,
    char** out_error);

int32_t sealed_kernel_bridge_get_semantic_context_min_bytes(
    uint64_t* out_bytes,
    char** out_error);

int32_t sealed_kernel_bridge_get_ai_chat_timeout_secs(
    uint64_t* out_secs,
    char** out_error);

int32_t sealed_kernel_bridge_get_ai_ponder_timeout_secs(
    uint64_t* out_secs,
    char** out_error);

int32_t sealed_kernel_bridge_get_ai_embedding_request_timeout_secs(
    uint64_t* out_secs,
    char** out_error);

int32_t sealed_kernel_bridge_get_ai_embedding_cache_limit(
    uint64_t* out_limit,
    char** out_error);

int32_t sealed_kernel_bridge_get_ai_embedding_concurrency_limit(
    uint64_t* out_limit,
    char** out_error);

int32_t sealed_kernel_bridge_get_ai_rag_top_note_limit(
    uint64_t* out_limit,
    char** out_error);

int32_t sealed_kernel_bridge_normalize_ai_embedding_text(
    const char* text_utf8,
    uint64_t text_size,
    char** out_text,
    char** out_error);

int32_t sealed_kernel_bridge_is_ai_embedding_text_indexable(
    const char* text_utf8,
    uint64_t text_size,
    uint8_t* out_is_indexable,
    char** out_error);

int32_t sealed_kernel_bridge_should_refresh_ai_embedding_note(
    int64_t note_updated_at,
    uint8_t has_existing_updated_at,
    int64_t existing_updated_at,
    uint8_t* out_should_refresh,
    char** out_error);

int32_t sealed_kernel_bridge_compute_ai_embedding_cache_key(
    const char* base_url_utf8,
    uint64_t base_url_size,
    const char* model_utf8,
    uint64_t model_size,
    const char* text_utf8,
    uint64_t text_size,
    char** out_key,
    char** out_error);

int32_t sealed_kernel_bridge_serialize_ai_embedding_blob(
    const float* values,
    uint64_t value_count,
    uint8_t** out_bytes,
    uint64_t* out_size,
    char** out_error);

int32_t sealed_kernel_bridge_parse_ai_embedding_blob(
    const uint8_t* blob,
    uint64_t blob_size,
    float** out_values,
    uint64_t* out_count,
    char** out_error);

int32_t sealed_kernel_bridge_upsert_ai_embedding_note_metadata(
    sealed_kernel_bridge_session* session,
    const char* rel_path_utf8,
    const char* title_utf8,
    const char* absolute_path_utf8,
    int64_t created_at,
    int64_t updated_at,
    char** out_error);

int32_t sealed_kernel_bridge_query_ai_embedding_note_timestamps_json(
    sealed_kernel_bridge_session* session,
    char** out_json,
    char** out_error);

int32_t sealed_kernel_bridge_update_ai_embedding(
    sealed_kernel_bridge_session* session,
    const char* rel_path_utf8,
    const float* values,
    uint64_t value_count,
    char** out_error);

int32_t sealed_kernel_bridge_clear_ai_embeddings(
    sealed_kernel_bridge_session* session,
    char** out_error);

int32_t sealed_kernel_bridge_delete_ai_embedding_note(
    sealed_kernel_bridge_session* session,
    const char* rel_path_utf8,
    uint8_t* out_deleted,
    char** out_error);

int32_t sealed_kernel_bridge_query_ai_embedding_top_notes_json(
    sealed_kernel_bridge_session* session,
    const float* query_values,
    uint64_t query_value_count,
    const char* exclude_rel_path_utf8,
    uint64_t limit,
    char** out_json,
    char** out_error);

int32_t sealed_kernel_bridge_build_ai_rag_system_content_text(
    const char* context_utf8,
    uint64_t context_size,
    char** out_text,
    char** out_error);

int32_t sealed_kernel_bridge_build_ai_rag_context_from_note_paths_text(
    const char* const* note_paths_utf8,
    const uint64_t* note_path_sizes,
    const char* const* note_contents_utf8,
    const uint64_t* note_content_sizes,
    uint64_t note_count,
    char** out_text,
    char** out_error);

int32_t sealed_kernel_bridge_get_ai_ponder_system_prompt_text(
    char** out_text,
    char** out_error);

int32_t sealed_kernel_bridge_build_ai_ponder_user_prompt_text(
    const char* topic_utf8,
    uint64_t topic_size,
    const char* context_utf8,
    uint64_t context_size,
    char** out_text,
    char** out_error);

int32_t sealed_kernel_bridge_get_ai_ponder_temperature(
    float* out_temperature,
    char** out_error);

int32_t sealed_kernel_bridge_compute_truth_state_json(
    const char* const* note_ids_utf8,
    const int64_t* active_secs,
    uint64_t activity_count,
    char** out_json,
    char** out_error);

int32_t sealed_kernel_bridge_compute_study_streak_days(
    const int64_t* day_buckets,
    uint64_t day_count,
    int64_t today_bucket,
    int64_t* out_streak_days,
    char** out_error);

int32_t sealed_kernel_bridge_compute_study_streak_days_from_timestamps(
    const int64_t* started_at_epoch_secs,
    uint64_t timestamp_count,
    int64_t today_bucket,
    int64_t* out_streak_days,
    char** out_error);

int32_t sealed_kernel_bridge_compute_study_stats_window(
    int64_t now_epoch_secs,
    int64_t days_back,
    int64_t* out_today_start_epoch_secs,
    int64_t* out_today_bucket,
    int64_t* out_week_start_epoch_secs,
    int64_t* out_daily_window_start_epoch_secs,
    int64_t* out_heatmap_start_epoch_secs,
    uint64_t* out_folder_rank_limit,
    char** out_error);

int32_t sealed_kernel_bridge_build_study_heatmap_grid_json(
    const char* const* dates_utf8,
    const int64_t* active_secs,
    uint64_t day_count,
    int64_t now_epoch_secs,
    char** out_json,
    char** out_error);

int32_t sealed_kernel_bridge_generate_mock_retrosynthesis_json(
    const char* target_smiles_utf8,
    uint8_t depth,
    char** out_json,
    char** out_error);

int32_t sealed_kernel_bridge_simulate_polymerization_kinetics_json(
    double m0,
    double i0,
    double cta0,
    double kd,
    double kp,
    double kt,
    double ktr,
    double time_max,
    uint64_t steps,
    char** out_json,
    char** out_error);

int32_t sealed_kernel_bridge_recalculate_stoichiometry_json(
    const double* mw,
    const double* eq,
    const double* moles,
    const double* mass,
    const double* volume,
    const double* density,
    const uint8_t* has_density,
    const uint8_t* is_reference,
    uint64_t count,
    char** out_json,
    char** out_error);

int32_t sealed_kernel_bridge_get_pdf_ink_default_tolerance(
    float* out_tolerance,
    char** out_error);

int32_t sealed_kernel_bridge_compute_pdf_annotation_storage_key(
    const char* pdf_path_utf8,
    char** out_key,
    char** out_error);

int32_t sealed_kernel_bridge_compute_pdf_lightweight_hash(
    const uint8_t* head,
    uint64_t head_size,
    const uint8_t* tail,
    uint64_t tail_size,
    uint64_t file_size,
    char** out_hash,
    char** out_error);

int32_t sealed_kernel_bridge_smooth_ink_strokes_json(
    const float* xs,
    const float* ys,
    const float* pressures,
    const uint64_t* point_counts,
    const float* stroke_widths,
    uint64_t stroke_count,
    float tolerance,
    char** out_json,
    char** out_error);

int32_t sealed_kernel_bridge_build_molecular_preview_json(
    const char* raw_utf8,
    uint64_t raw_size,
    const char* extension_utf8,
    uint64_t max_atoms,
    char** out_json,
    char** out_error);

int32_t sealed_kernel_bridge_normalize_molecular_preview_atom_limit(
    uint64_t requested_atoms,
    uint64_t* out_atoms,
    char** out_error);

int32_t sealed_kernel_bridge_get_symmetry_atom_limit(
    uint64_t* out_atoms,
    char** out_error);

int32_t sealed_kernel_bridge_get_crystal_supercell_atom_limit(
    uint64_t* out_atoms,
    char** out_error);

int32_t sealed_kernel_bridge_calculate_symmetry_json(
    const char* raw_utf8,
    uint64_t raw_size,
    const char* format_utf8,
    uint64_t max_atoms,
    char** out_json,
    char** out_error);

int32_t sealed_kernel_bridge_build_lattice_from_cif_json(
    const char* raw_utf8,
    uint64_t raw_size,
    uint32_t nx,
    uint32_t ny,
    uint32_t nz,
    char** out_json,
    char** out_error);

int32_t sealed_kernel_bridge_calculate_miller_plane_from_cif_json(
    const char* raw_utf8,
    uint64_t raw_size,
    int32_t h,
    int32_t k,
    int32_t l,
    char** out_json,
    char** out_error);

int32_t sealed_kernel_bridge_relativize_vault_path_text(
    sealed_kernel_bridge_session* session,
    const char* host_path_utf8,
    uint64_t host_path_size,
    uint8_t allow_empty,
    char** out_text,
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
