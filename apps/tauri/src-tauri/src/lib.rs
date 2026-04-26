mod ai;
mod chem_api;
mod commands;
mod compiler;
mod crystal;
mod db;
mod error;
mod kinetics;
mod models;
mod pdf;
mod sealed_kernel;
mod shared;
mod symmetry;
mod watcher;

pub use error::{AppError, AppResult};

use std::sync::{Arc, Mutex};

use db::DbState;
use tauri::Manager;

/// Tauri 应用入口配置
///
/// # 数据库生命周期管理
/// 数据库的初始化时机是在前端调用 `scan_vault` 时（即用户选择知识库目录后），
/// 而非应用启动时。这是因为数据库文件存储在 Vault 目录下，
/// 启动时尚不知道用户要打开哪个 Vault。
///
/// 因此这里使用 `setup` 钩子注册一个"空的" DbState 占位，
/// 实际的数据库连接会在 `init_vault` 命令中被替换。
///
/// 但为了简化当前阶段的实现，我们采用另一种方案：
/// 新增一个 `init_vault` 命令，前端在选择目录后先调用它初始化数据库，
/// 再调用 `scan_vault` 扫描文件。
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
        // 注册一个初始的空数据库状态
        // 使用内存数据库作为占位，init_vault 命令会替换为真实的文件数据库
        .setup(|app| {
            let placeholder_conn =
                rusqlite::Connection::open_in_memory().expect("创建占位内存数据库失败");
            app.manage(DbState {
                conn: Arc::new(Mutex::new(placeholder_conn)),
            });
            app.manage(ai::EmbeddingRuntimeState::default());
            app.manage(ai::VectorCacheState::default());
            app.manage(compiler::CompilerState::detect());
            app.manage(watcher::WatcherState::new());
            app.manage(sealed_kernel::SealedKernelState::default());

            Ok(())
        })
        .run(tauri::generate_context!())
        .expect("启动 Tauri 应用时发生错误");
}
