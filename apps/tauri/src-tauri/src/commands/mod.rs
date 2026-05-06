pub(crate) mod cmd_ai;
pub(crate) mod cmd_chem;
pub(crate) mod cmd_compiler;
pub(crate) mod cmd_compute;
pub(crate) mod cmd_crystal;
pub(crate) mod cmd_media;
pub(crate) mod cmd_pdf;
pub(crate) mod cmd_search;
pub(crate) mod cmd_study;
pub(crate) mod cmd_symmetry;
pub(crate) mod cmd_tree;
pub(crate) mod cmd_vault;
pub(crate) mod cmd_vault_embeddings;
pub(crate) mod cmd_vault_entries;

macro_rules! app_invoke_handler {
    () => {
        tauri::generate_handler![
            $crate::commands::cmd_vault::init_vault,
            $crate::commands::cmd_compiler::get_compiler_status,
            $crate::commands::cmd_compiler::compile_to_pdf,
            $crate::commands::cmd_search::search_notes,
            $crate::commands::cmd_search::get_backlinks,
            $crate::commands::cmd_search::semantic_search,
            $crate::commands::cmd_search::get_related_notes,
            $crate::commands::cmd_search::get_graph_data,
            $crate::commands::cmd_search::get_all_tags,
            $crate::commands::cmd_search::get_notes_by_tag,
            $crate::commands::cmd_tree::build_file_tree,
            $crate::commands::cmd_ai::ask_vault,
            $crate::commands::cmd_ai::test_ai_connection,
            $crate::commands::cmd_ai::ponder_node,
            $crate::commands::cmd_compute::compute_truth_diff,
            $crate::commands::cmd_compute::build_semantic_context,
            $crate::commands::cmd_compute::recalculate_stoichiometry,
            $crate::commands::cmd_compute::normalize_database,
            $crate::commands::cmd_search::get_related_notes_raw,
            $crate::commands::cmd_search::get_tag_tree,
            $crate::commands::cmd_search::get_enriched_graph_data,
            $crate::commands::cmd_study::get_heatmap_cells,
            $crate::commands::cmd_study::study_session_start,
            $crate::commands::cmd_study::study_session_tick,
            $crate::commands::cmd_study::study_session_end,
            $crate::commands::cmd_study::study_stats_query,
            $crate::commands::cmd_study::truth_state_from_study,
            $crate::commands::cmd_chem::fetch_compound_info,
            $crate::commands::cmd_chem::retrosynthesize_target,
            $crate::commands::cmd_chem::simulate_polymerization,
            $crate::commands::cmd_vault::scan_vault,
            $crate::commands::cmd_vault_embeddings::index_vault_content,
            $crate::commands::cmd_vault_embeddings::rebuild_vector_index,
            $crate::commands::cmd_media::read_note,
            $crate::commands::cmd_media::read_molecular_preview,
            $crate::commands::cmd_media::read_note_indexed_content,
            $crate::commands::cmd_media::read_binary_file,
            $crate::commands::cmd_media::parse_spectroscopy,
            $crate::commands::cmd_symmetry::calculate_symmetry,
            $crate::commands::cmd_crystal::parse_and_build_lattice,
            $crate::commands::cmd_crystal::calculate_miller_plane,
            $crate::commands::cmd_vault::write_note,
            $crate::commands::cmd_vault_entries::delete_entry,
            $crate::commands::cmd_vault_entries::move_entry,
            $crate::commands::cmd_vault_entries::rename_entry,
            $crate::commands::cmd_vault_entries::create_folder,
            $crate::sealed_kernel::sealed_kernel_bridge_info,
            $crate::sealed_kernel::sealed_kernel_open_vault,
            $crate::sealed_kernel::sealed_kernel_get_state,
            $crate::sealed_kernel::sealed_kernel_close_vault,
            $crate::sealed_kernel::sealed_kernel_query_notes,
            $crate::sealed_kernel::sealed_kernel_query_chem_spectra,
            $crate::sealed_kernel::sealed_kernel_get_chem_spectrum,
            $crate::sealed_kernel::sealed_kernel_query_note_chem_spectrum_refs,
            $crate::sealed_kernel::sealed_kernel_query_chem_spectrum_referrers,
            $crate::commands::cmd_pdf::read_pdf_file,
            $crate::commands::cmd_pdf::smooth_ink_strokes,
            $crate::commands::cmd_pdf::load_pdf_annotations,
            $crate::commands::cmd_pdf::save_pdf_annotations,
            $crate::commands::cmd_vault::scan_changed_entries,
            $crate::commands::cmd_vault_embeddings::index_changed_entries,
            $crate::commands::cmd_vault::remove_deleted_entries,
            $crate::commands::cmd_vault::start_watcher,
            $crate::commands::cmd_vault::stop_watcher
        ]
    };
}

pub(crate) use app_invoke_handler;
