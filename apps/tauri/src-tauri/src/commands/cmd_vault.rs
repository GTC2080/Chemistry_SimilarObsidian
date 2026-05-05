use tauri::{AppHandle, State};

use crate::models::NoteInfo;
use crate::sealed_kernel::{self, SealedKernelState};
use crate::watcher::WatcherState;
use crate::AppError;

#[tauri::command]
pub fn init_vault(
    vault_path: String,
    sealed_kernel: State<SealedKernelState>,
) -> Result<(), AppError> {
    init_vault_note_workflow(&vault_path, sealed_kernel.inner())
}

fn init_vault_note_workflow(
    vault_path: &str,
    sealed_kernel: &SealedKernelState,
) -> Result<(), AppError> {
    sealed_kernel::ensure_vault_open(vault_path, sealed_kernel)
}

// ---------------------------------------------------------------------------
// scan_vault — fast metadata-only walk (Phase 1)
// ---------------------------------------------------------------------------

/// Query kernel note metadata immediately.
/// Does NOT read file content, extract PDF text, or upsert to the database.
/// This lets the frontend show the file tree as fast as possible.
#[tauri::command]
pub fn scan_vault(
    vault_path: String,
    ignored_folders: Option<String>,
    sealed_kernel: State<SealedKernelState>,
) -> Result<Vec<NoteInfo>, AppError> {
    sealed_kernel::validate_vault_root_path(&vault_path)
        .map_err(|_| AppError::Custom(format!("路径不存在或不是一个有效目录: {}", vault_path)))?;
    sealed_kernel::query_note_infos_filtered(
        &vault_path,
        sealed_kernel.inner(),
        sealed_kernel::vault_scan_default_limit()?,
        ignored_folders.as_deref().unwrap_or(""),
    )
}

#[tauri::command]
pub async fn write_note(
    vault_path: String,
    file_path: String,
    content: String,
    sealed_kernel: State<'_, SealedKernelState>,
) -> Result<(), AppError> {
    sealed_kernel::write_note_by_file_path(
        &vault_path,
        &file_path,
        &content,
        sealed_kernel.inner(),
    )?;
    Ok(())
}

// ---------------------------------------------------------------------------
// Incremental scan / index — 增量扫描与索引（供 watcher 调用）
// ---------------------------------------------------------------------------

/// 只扫描指定的相对路径列表，返回对应的 NoteInfo。
/// 路径不存在或不支持的文件会被静默跳过。
#[tauri::command]
pub fn scan_changed_entries(
    vault_path: String,
    paths: Vec<String>,
    sealed_kernel: State<SealedKernelState>,
) -> Result<Vec<NoteInfo>, AppError> {
    sealed_kernel::query_changed_note_infos(&vault_path, sealed_kernel.inner(), &paths)
}

/// 从数据库中删除指定相对路径列表对应的笔记。
#[tauri::command]
pub fn remove_deleted_entries(
    paths: Vec<String>,
    sealed_kernel: State<SealedKernelState>,
) -> Result<u32, AppError> {
    let removed = sealed_kernel::delete_changed_ai_embedding_notes(&paths, sealed_kernel.inner())?;
    u32::try_from(removed).map_err(|_| AppError::Custom("删除数量超过前端可表示范围".to_string()))
}

// ---------------------------------------------------------------------------
// File watcher commands — 增量文件监听
// ---------------------------------------------------------------------------

/// 启动文件系统监听。vault 打开后调用，持续监听文件变更。
#[tauri::command]
pub fn start_watcher(
    vault_path: String,
    ignored_folders: Option<String>,
    app: AppHandle,
    watcher: State<WatcherState>,
    sealed_kernel: State<SealedKernelState>,
) -> Result<(), AppError> {
    sealed_kernel::ensure_vault_open(&vault_path, sealed_kernel.inner())?;
    let ignored_roots = ignored_folders.unwrap_or_default();
    watcher
        .start(&vault_path, &ignored_roots, app)
        .map_err(AppError::Custom)
}

/// 停止文件系统监听。切换 vault 或关闭时调用。
#[tauri::command]
pub fn stop_watcher(watcher: State<WatcherState>) -> Result<(), AppError> {
    watcher.stop();
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    fn make_temp_vault(prefix: &str) -> std::path::PathBuf {
        let unique = format!(
            "{}_{}",
            prefix,
            std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .expect("system clock")
                .as_nanos()
        );
        let path = std::env::temp_dir().join(unique);
        std::fs::create_dir_all(&path).expect("create temp vault");
        path
    }

    #[test]
    fn init_vault_note_workflow_does_not_create_legacy_index_db() {
        let vault = make_temp_vault("nexus_init_vault_kernel_only");
        let vault_path = vault.to_string_lossy().into_owned();
        let state = SealedKernelState::default();

        init_vault_note_workflow(&vault_path, &state).expect("open vault through kernel");

        assert!(sealed_kernel::active_session_token(&state).is_ok());
        assert!(
            !vault.join("index.db").exists(),
            "init_vault must not create the legacy Rust index.db"
        );

        sealed_kernel::close_vault_state(&state).expect("close kernel vault");
        let _ = std::fs::remove_dir_all(vault);
    }
}
