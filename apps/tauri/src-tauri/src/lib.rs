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

fn manage_runtime_state(app: &mut tauri::App) {
    app.manage(ai::EmbeddingRuntimeState::default());
    app.manage(compiler::CompilerState::detect());
    app.manage(watcher::WatcherState::new());
    app.manage(sealed_kernel::SealedKernelState::default());
}

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
        .invoke_handler(commands::app_invoke_handler!())
        .setup(|app| {
            manage_runtime_state(app);
            Ok(())
        })
        .run(tauri::generate_context!())
        .expect("启动 Tauri 应用时发生错误");
}
