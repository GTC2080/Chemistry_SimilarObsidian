/* Reason: This file exposes the frozen minimal C ABI entry points for the Phase 1 kernel skeleton. */

#pragma once

#include "kernel/types.h"

#ifdef __cplusplus
extern "C" {
#endif

kernel_status kernel_open_vault(const char* vault_path, kernel_handle** out_handle);
kernel_status kernel_close(kernel_handle* handle);
kernel_status kernel_get_state(kernel_handle* handle, kernel_state_snapshot* out_state);
kernel_status kernel_get_rebuild_status(
    kernel_handle* handle,
    kernel_rebuild_status_snapshot* out_status);
kernel_status kernel_read_note(
    kernel_handle* handle,
    const char* rel_path,
    kernel_owned_buffer* out_buffer,
    kernel_note_metadata* out_metadata);
kernel_status kernel_query_notes(
    kernel_handle* handle,
    size_t limit,
    kernel_note_list* out_notes);
kernel_status kernel_query_file_tree(
    kernel_handle* handle,
    size_t limit,
    kernel_file_tree* out_tree);
kernel_status kernel_write_note(
    kernel_handle* handle,
    const char* rel_path,
    const char* utf8_text,
    size_t text_size,
    const char* expected_revision,
    kernel_note_metadata* out_metadata,
    kernel_write_disposition* out_disposition);
kernel_status kernel_create_folder(kernel_handle* handle, const char* folder_rel_path);
kernel_status kernel_delete_entry(kernel_handle* handle, const char* target_rel_path);
kernel_status kernel_rename_entry(
    kernel_handle* handle,
    const char* source_rel_path,
    const char* new_name);
kernel_status kernel_move_entry(
    kernel_handle* handle,
    const char* source_rel_path,
    const char* dest_folder_rel_path);
kernel_status kernel_search_notes(
    kernel_handle* handle,
    const char* query,
    kernel_search_results* out_results);
kernel_status kernel_search_notes_limited(
    kernel_handle* handle,
    const char* query,
    size_t limit,
    kernel_search_results* out_results);
kernel_status kernel_query_search(
    kernel_handle* handle,
    const kernel_search_query* request,
    kernel_search_page* out_page);
kernel_status kernel_query_tag_notes(
    kernel_handle* handle,
    const char* tag,
    size_t limit,
    kernel_search_results* out_results);
kernel_status kernel_query_tags(
    kernel_handle* handle,
    size_t limit,
    kernel_tag_list* out_tags);
kernel_status kernel_query_graph(
    kernel_handle* handle,
    size_t note_limit,
    kernel_graph* out_graph);
kernel_status kernel_query_backlinks(
    kernel_handle* handle,
    const char* rel_path,
    size_t limit,
    kernel_search_results* out_results);
kernel_status kernel_query_attachments(
    kernel_handle* handle,
    size_t limit,
    kernel_attachment_list* out_attachments);
kernel_status kernel_get_attachment(
    kernel_handle* handle,
    const char* attachment_rel_path,
    kernel_attachment_record* out_attachment);
kernel_status kernel_query_note_attachment_refs(
    kernel_handle* handle,
    const char* note_rel_path,
    size_t limit,
    kernel_attachment_list* out_attachments);
kernel_status kernel_query_attachment_referrers(
    kernel_handle* handle,
    const char* attachment_rel_path,
    size_t limit,
    kernel_attachment_referrers* out_referrers);
kernel_status kernel_get_pdf_metadata(
    kernel_handle* handle,
    const char* attachment_rel_path,
    kernel_pdf_metadata_record* out_metadata);
kernel_status kernel_query_note_pdf_source_refs(
    kernel_handle* handle,
    const char* note_rel_path,
    size_t limit,
    kernel_pdf_source_refs* out_refs);
kernel_status kernel_query_pdf_referrers(
    kernel_handle* handle,
    const char* attachment_rel_path,
    size_t limit,
    kernel_pdf_referrers* out_referrers);
kernel_status kernel_smooth_ink_strokes(
    const kernel_ink_stroke_input* strokes,
    size_t stroke_count,
    float tolerance,
    kernel_ink_smoothing_result* out_result);
kernel_status kernel_query_attachment_domain_metadata(
    kernel_handle* handle,
    const char* attachment_rel_path,
    size_t limit,
    kernel_domain_metadata_list* out_entries);
kernel_status kernel_query_chem_spectrum_metadata(
    kernel_handle* handle,
    const char* attachment_rel_path,
    size_t limit,
    kernel_domain_metadata_list* out_entries);
kernel_status kernel_query_chem_spectra(
    kernel_handle* handle,
    size_t limit,
    kernel_chem_spectrum_list* out_spectra);
kernel_status kernel_get_chem_spectrum(
    kernel_handle* handle,
    const char* attachment_rel_path,
    kernel_chem_spectrum_record* out_spectrum);
kernel_status kernel_simulate_polymerization_kinetics(
    const kernel_polymerization_kinetics_params* params,
    kernel_polymerization_kinetics_result* out_result);
kernel_status kernel_recalculate_stoichiometry(
    const kernel_stoichiometry_row_input* rows,
    size_t count,
    kernel_stoichiometry_row_output* out_rows);
kernel_status kernel_generate_mock_retrosynthesis(
    const char* target_smiles,
    uint8_t depth,
    kernel_retro_tree* out_tree);
kernel_status kernel_parse_spectroscopy_text(
    const char* raw,
    size_t raw_size,
    const char* extension,
    kernel_spectroscopy_data* out_data);
kernel_status kernel_build_molecular_preview(
    const char* raw,
    size_t raw_size,
    const char* extension,
    size_t max_atoms,
    kernel_molecular_preview* out_preview);
kernel_status kernel_parse_cif_crystal(
    const char* raw,
    size_t raw_size,
    kernel_crystal_parse_result* out_result);
kernel_status kernel_calculate_miller_plane(
    const kernel_crystal_cell_params* cell,
    int32_t h,
    int32_t k,
    int32_t l,
    kernel_miller_plane_result* out_result);
kernel_status kernel_build_supercell(
    const kernel_crystal_cell_params* cell,
    const kernel_fractional_atom_input* atoms,
    size_t atom_count,
    const kernel_symmetry_operation_input* symops,
    size_t symop_count,
    uint32_t nx,
    uint32_t ny,
    uint32_t nz,
    kernel_supercell_result* out_result);
kernel_status kernel_build_lattice_from_cif(
    const char* raw,
    size_t raw_size,
    uint32_t nx,
    uint32_t ny,
    uint32_t nz,
    kernel_lattice_result* out_result);
kernel_status kernel_calculate_miller_plane_from_cif(
    const char* raw,
    size_t raw_size,
    int32_t h,
    int32_t k,
    int32_t l,
    kernel_cif_miller_plane_result* out_result);
kernel_status kernel_classify_point_group(
    const kernel_symmetry_axis_input* axes,
    size_t axis_count,
    const kernel_symmetry_plane_input* planes,
    size_t plane_count,
    uint8_t has_inversion,
    kernel_symmetry_classification_result* out_result);
kernel_status kernel_analyze_symmetry_shape(
    const kernel_symmetry_atom_input* atoms,
    size_t atom_count,
    kernel_symmetry_shape_result* out_result);
kernel_status kernel_compute_symmetry_principal_axes(
    const kernel_symmetry_atom_input* atoms,
    size_t atom_count,
    kernel_symmetry_direction_input* out_axes);
kernel_status kernel_find_symmetry_rotation_axes(
    const kernel_symmetry_atom_input* atoms,
    size_t atom_count,
    const kernel_symmetry_direction_input* candidates,
    size_t candidate_count,
    kernel_symmetry_axis_input* out_axes,
    size_t out_axis_capacity,
    size_t* out_axis_count);
kernel_status kernel_generate_symmetry_candidate_directions(
    const kernel_symmetry_atom_input* atoms,
    size_t atom_count,
    const kernel_symmetry_direction_input* principal_axes,
    size_t principal_axis_count,
    kernel_symmetry_direction_input* out_directions,
    size_t out_direction_capacity,
    size_t* out_direction_count);
kernel_status kernel_generate_symmetry_candidate_planes(
    const kernel_symmetry_atom_input* atoms,
    size_t atom_count,
    const kernel_symmetry_axis_input* found_axes,
    size_t axis_count,
    const kernel_symmetry_direction_input* principal_axes,
    size_t principal_axis_count,
    kernel_symmetry_plane_input* out_planes,
    size_t out_plane_capacity,
    size_t* out_plane_count);
kernel_status kernel_find_symmetry_mirror_planes(
    const kernel_symmetry_atom_input* atoms,
    size_t atom_count,
    const kernel_symmetry_plane_input* candidates,
    size_t candidate_count,
    kernel_symmetry_plane_input* out_planes,
    size_t out_plane_capacity,
    size_t* out_plane_count);
kernel_status kernel_build_symmetry_render_geometry(
    const kernel_symmetry_axis_input* axes,
    size_t axis_count,
    const kernel_symmetry_plane_input* planes,
    size_t plane_count,
    double mol_radius,
    kernel_symmetry_render_axis* out_axes,
    kernel_symmetry_render_plane* out_planes);
kernel_status kernel_parse_symmetry_atoms_text(
    const char* raw,
    size_t raw_size,
    const char* format,
    kernel_symmetry_atom_list* out_atoms);
kernel_status kernel_calculate_symmetry(
    const char* raw,
    size_t raw_size,
    const char* format,
    size_t max_atoms,
    kernel_symmetry_calculation_result* out_result);
kernel_status kernel_query_note_chem_spectrum_refs(
    kernel_handle* handle,
    const char* note_rel_path,
    size_t limit,
    kernel_chem_spectrum_source_refs* out_refs);
kernel_status kernel_query_chem_spectrum_referrers(
    kernel_handle* handle,
    const char* attachment_rel_path,
    size_t limit,
    kernel_chem_spectrum_referrers* out_referrers);
kernel_status kernel_query_pdf_domain_metadata(
    kernel_handle* handle,
    const char* attachment_rel_path,
    size_t limit,
    kernel_domain_metadata_list* out_entries);
kernel_status kernel_query_attachment_domain_objects(
    kernel_handle* handle,
    const char* attachment_rel_path,
    size_t limit,
    kernel_domain_object_list* out_objects);
kernel_status kernel_query_pdf_domain_objects(
    kernel_handle* handle,
    const char* attachment_rel_path,
    size_t limit,
    kernel_domain_object_list* out_objects);
kernel_status kernel_get_domain_object(
    kernel_handle* handle,
    const char* domain_object_key,
    kernel_domain_object_descriptor* out_object);
kernel_status kernel_query_note_domain_source_refs(
    kernel_handle* handle,
    const char* note_rel_path,
    size_t limit,
    kernel_domain_source_refs* out_refs);
kernel_status kernel_query_domain_object_referrers(
    kernel_handle* handle,
    const char* domain_object_key,
    size_t limit,
    kernel_domain_referrers* out_referrers);
kernel_status kernel_list_note_attachments(
    kernel_handle* handle,
    const char* note_rel_path,
    kernel_attachment_refs* out_refs);
kernel_status kernel_get_attachment_metadata(
    kernel_handle* handle,
    const char* attachment_rel_path,
    kernel_attachment_metadata* out_metadata);
kernel_status kernel_start_rebuild_index(kernel_handle* handle);
kernel_status kernel_wait_for_rebuild(kernel_handle* handle, uint32_t timeout_ms);
kernel_status kernel_join_rebuild_index(kernel_handle* handle);
kernel_status kernel_rebuild_index(kernel_handle* handle);
kernel_status kernel_export_diagnostics(kernel_handle* handle, const char* output_path);
void kernel_free_buffer(kernel_owned_buffer* buffer);
void kernel_free_note_list(kernel_note_list* notes);
void kernel_free_file_tree(kernel_file_tree* tree);
void kernel_free_tag_list(kernel_tag_list* tags);
void kernel_free_graph(kernel_graph* graph);
void kernel_free_search_results(kernel_search_results* results);
void kernel_free_search_page(kernel_search_page* page);
void kernel_free_attachment_record(kernel_attachment_record* attachment);
void kernel_free_attachment_list(kernel_attachment_list* attachments);
void kernel_free_attachment_referrers(kernel_attachment_referrers* referrers);
void kernel_free_attachment_refs(kernel_attachment_refs* refs);
void kernel_free_pdf_metadata_record(kernel_pdf_metadata_record* metadata);
void kernel_free_pdf_source_refs(kernel_pdf_source_refs* refs);
void kernel_free_pdf_referrers(kernel_pdf_referrers* referrers);
void kernel_free_ink_smoothing_result(kernel_ink_smoothing_result* result);
void kernel_free_domain_metadata_list(kernel_domain_metadata_list* entries);
void kernel_free_domain_object_descriptor(kernel_domain_object_descriptor* object);
void kernel_free_domain_object_list(kernel_domain_object_list* objects);
void kernel_free_chem_spectrum_record(kernel_chem_spectrum_record* spectrum);
void kernel_free_chem_spectrum_list(kernel_chem_spectrum_list* spectra);
void kernel_free_polymerization_kinetics_result(kernel_polymerization_kinetics_result* result);
void kernel_free_retro_tree(kernel_retro_tree* tree);
void kernel_free_spectroscopy_data(kernel_spectroscopy_data* data);
void kernel_free_molecular_preview(kernel_molecular_preview* preview);
void kernel_free_crystal_parse_result(kernel_crystal_parse_result* result);
void kernel_free_supercell_result(kernel_supercell_result* result);
void kernel_free_lattice_result(kernel_lattice_result* result);
void kernel_free_symmetry_atom_list(kernel_symmetry_atom_list* atoms);
void kernel_free_symmetry_calculation_result(kernel_symmetry_calculation_result* result);
void kernel_free_chem_spectrum_source_refs(kernel_chem_spectrum_source_refs* refs);
void kernel_free_chem_spectrum_referrers(kernel_chem_spectrum_referrers* referrers);
void kernel_free_domain_source_refs(kernel_domain_source_refs* refs);
void kernel_free_domain_referrers(kernel_domain_referrers* referrers);

#ifdef __cplusplus
}
#endif
