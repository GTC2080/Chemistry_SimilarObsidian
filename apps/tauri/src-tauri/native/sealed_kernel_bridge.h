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

int32_t sealed_kernel_bridge_get_note_catalog_default_limit(
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
