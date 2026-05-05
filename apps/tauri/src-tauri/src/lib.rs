mod ai;
mod chem_api;
mod commands;
mod compiler;
mod crystal;
mod error;
mod kinetics;
mod models;
mod pdf;
mod sealed_kernel;
mod shared;
mod symmetry;
mod watcher;

pub use error::{AppError, AppResult};

use tauri::Manager;

/// Tauri 应用入口配置
///
/// # 后端生命周期管理
/// 笔记、文件树、搜索、embedding 和 study/session 主链路由 sealed kernel 打开 vault。
/// Tauri Rust 只注册命令、桥接 kernel、管理平台插件和外部网络/文件监听胶水。
#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_dialog::init())
        .plugin(tauri_plugin_store::Builder::default().build())
        .invoke_handler(tauri::generate_handler![
            commands::cmd_vault::init_vault,
            commands::cmd_compiler::get_compiler_status,
            commands::cmd_compiler::compile_to_pdf,
            commands::cmd_search::search_notes,
            commands::cmd_search::get_backlinks,
            commands::cmd_search::semantic_search,
            commands::cmd_search::get_related_notes,
            commands::cmd_search::get_graph_data,
            commands::cmd_search::get_all_tags,
            commands::cmd_search::get_notes_by_tag,
            commands::cmd_tree::build_file_tree,
            commands::cmd_ai::ask_vault,
            commands::cmd_ai::test_ai_connection,
            commands::cmd_ai::ponder_node,
            commands::cmd_compute::compute_truth_diff,
            commands::cmd_compute::build_semantic_context,
            commands::cmd_compute::recalculate_stoichiometry,
            commands::cmd_compute::normalize_database,
            commands::cmd_search::get_related_notes_raw,
            commands::cmd_search::get_tag_tree,
            commands::cmd_search::get_enriched_graph_data,
            commands::cmd_study::get_heatmap_cells,
            commands::cmd_study::study_session_start,
            commands::cmd_study::study_session_tick,
            commands::cmd_study::study_session_end,
            commands::cmd_study::study_stats_query,
            commands::cmd_study::truth_state_from_study,
            commands::cmd_chem::fetch_compound_info,
            commands::cmd_chem::retrosynthesize_target,
            commands::cmd_chem::simulate_polymerization,
            commands::cmd_vault::scan_vault,
            commands::cmd_vault::index_vault_content,
            commands::cmd_vault::rebuild_vector_index,
            commands::cmd_media::read_note,
            commands::cmd_media::read_molecular_preview,
            commands::cmd_media::read_note_indexed_content,
            commands::cmd_media::read_binary_file,
            commands::cmd_media::parse_spectroscopy,
            commands::cmd_symmetry::calculate_symmetry,
            commands::cmd_crystal::parse_and_build_lattice,
            commands::cmd_crystal::calculate_miller_plane,
            commands::cmd_vault::write_note,
            commands::cmd_vault_entries::delete_entry,
            commands::cmd_vault_entries::move_entry,
            commands::cmd_vault_entries::rename_entry,
            commands::cmd_vault_entries::create_folder,
            sealed_kernel::sealed_kernel_bridge_info,
            sealed_kernel::sealed_kernel_open_vault,
            sealed_kernel::sealed_kernel_get_state,
            sealed_kernel::sealed_kernel_close_vault,
            sealed_kernel::sealed_kernel_query_notes,
            sealed_kernel::sealed_kernel_query_chem_spectra,
            sealed_kernel::sealed_kernel_get_chem_spectrum,
            sealed_kernel::sealed_kernel_query_note_chem_spectrum_refs,
            sealed_kernel::sealed_kernel_query_chem_spectrum_referrers,
            commands::cmd_pdf::read_pdf_file,
            commands::cmd_pdf::smooth_ink_strokes,
            commands::cmd_pdf::load_pdf_annotations,
            commands::cmd_pdf::save_pdf_annotations,
            commands::cmd_vault::scan_changed_entries,
            commands::cmd_vault::index_changed_entries,
            commands::cmd_vault::remove_deleted_entries,
            commands::cmd_vault::start_watcher,
            commands::cmd_vault::stop_watcher
        ])
        .setup(|app| {
            app.manage(ai::EmbeddingRuntimeState::default());
            app.manage(compiler::CompilerState::detect());
            app.manage(watcher::WatcherState::new());
            app.manage(sealed_kernel::SealedKernelState::default());

            Ok(())
        })
        .run(tauri::generate_context!())
        .expect("启动 Tauri 应用时发生错误");
}
