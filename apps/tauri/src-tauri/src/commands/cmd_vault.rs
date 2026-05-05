use std::collections::{HashMap, HashSet};

use futures_util::stream::{self, StreamExt};
use tauri::{AppHandle, State};

use crate::ai;
use crate::models::NoteInfo;
use crate::sealed_kernel::{self, SealedKernelState};
use crate::shared::command_utils::read_ai_config;
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

fn deleted_markdown_rel_paths(paths: &[String]) -> Result<Vec<String>, AppError> {
    sealed_kernel::filter_changed_markdown_paths(paths)
}

fn kernel_note_info_map_for_rel_paths(
    vault_path: &str,
    rel_paths: &[String],
    kernel_state: &SealedKernelState,
) -> Result<HashMap<String, NoteInfo>, AppError> {
    if rel_paths.is_empty() {
        return Ok(HashMap::new());
    }

    let wanted: HashSet<&str> = rel_paths.iter().map(String::as_str).collect();
    let limit = sealed_kernel::note_catalog_default_limit()?;
    let notes = sealed_kernel::query_note_infos(vault_path, kernel_state, limit)?;

    Ok(notes
        .into_iter()
        .filter(|note| wanted.contains(note.id.as_str()))
        .map(|note| (note.id.clone(), note))
        .collect())
}

fn spawn_embedding_tasks(
    refresh_jobs: Vec<sealed_kernel::AiEmbeddingRefreshJob>,
    ai_config: Option<ai::AiConfig>,
    kernel_session: usize,
    embedding_runtime: State<ai::EmbeddingRuntimeState>,
) {
    let Some(config) = ai_config else {
        return;
    };

    for job in refresh_jobs {
        let embedding_runtime = embedding_runtime.inner().clone();
        let note_id = job.id;
        let text_for_embedding = job.content;
        let config = config.clone();
        let version = embedding_runtime.bump_version(&note_id);

        tauri::async_runtime::spawn(async move {
            match ai::fetch_embedding_cached(&text_for_embedding, &config, &embedding_runtime).await
            {
                Ok(embedding) => {
                    if !embedding_runtime.is_current_version(&note_id, version) {
                        eprintln!("[向量化] 跳过过期结果 [{}] v{}", note_id, version);
                        return;
                    }
                    if let Err(e) = sealed_kernel::update_ai_embedding_for_session(
                        kernel_session,
                        &note_id,
                        &embedding,
                    ) {
                        eprintln!("[向量化] 写入失败 [{}]: {}", note_id, e);
                    } else {
                        eprintln!("[向量化] 成功 [{}]: {}维向量", note_id, embedding.len());
                    }
                }
                Err(e) => eprintln!("[向量化] 跳过 [{}]: {}", note_id, e),
            }
        });
    }
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

// ---------------------------------------------------------------------------
// index_vault_content — background content indexing (Phase 2)
// ---------------------------------------------------------------------------

/// Refresh the kernel-owned AI embedding cache from the kernel note catalog and
/// spawn embedding tasks. Rust no longer stores note embedding rows.
#[tauri::command]
pub fn index_vault_content(
    vault_path: String,
    ignored_folders: Option<String>,
    app: AppHandle,
    embedding_runtime: State<ai::EmbeddingRuntimeState>,
    sealed_kernel: State<SealedKernelState>,
) -> Result<u32, AppError> {
    let ai_config = read_ai_config(&app)
        .ok()
        .filter(|config| !config.api_key.trim().is_empty());

    sealed_kernel::ensure_vault_open(&vault_path, sealed_kernel.inner())?;
    let refresh_jobs = sealed_kernel::prepare_ai_embedding_refresh_jobs(
        ignored_folders.as_deref().unwrap_or(""),
        false,
        sealed_kernel.inner(),
    )?;

    let indexed_count = refresh_jobs.len() as u32;
    let kernel_session = sealed_kernel::active_session_token(sealed_kernel.inner())?;
    spawn_embedding_tasks(refresh_jobs, ai_config, kernel_session, embedding_runtime);

    Ok(indexed_count)
}

#[tauri::command]
pub async fn rebuild_vector_index(
    app: AppHandle,
    embedding_runtime: State<'_, ai::EmbeddingRuntimeState>,
    sealed_kernel: State<'_, SealedKernelState>,
) -> Result<u32, AppError> {
    let config = read_ai_config(&app)?;
    if config.api_key.trim().is_empty() {
        return Err(AppError::Custom(
            "未配置 AI API Key，无法重建向量索引".to_string(),
        ));
    }

    sealed_kernel::clear_ai_embeddings(sealed_kernel.inner())?;
    let all_notes =
        sealed_kernel::prepare_ai_embedding_refresh_jobs("", true, sealed_kernel.inner())?
            .into_iter()
            .map(|job| (job.id, job.content))
            .collect::<Vec<_>>();
    let kernel_session = sealed_kernel::active_session_token(sealed_kernel.inner())?;

    // Process embeddings concurrently with buffer_unordered(4)
    let results: Vec<_> = stream::iter(all_notes)
        .map(|(id, content)| {
            let config = config.clone();
            let embedding_runtime = embedding_runtime.inner().clone();
            async move {
                match ai::fetch_embedding_cached(&content, &config, &embedding_runtime).await {
                    Ok(embedding) => Some((id, embedding)),
                    Err(e) => {
                        eprintln!("[向量化] 跳过 [{}]: {}", id, e);
                        None
                    }
                }
            }
        })
        .buffer_unordered(4)
        .filter_map(|x| std::future::ready(x))
        .collect()
        .await;

    for (id, embedding) in &results {
        sealed_kernel::update_ai_embedding_for_session(kernel_session, id, embedding)?;
    }

    Ok(results.len() as u32)
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
    let rel_paths = sealed_kernel::filter_changed_markdown_paths(&paths)?;
    let mut notes_by_id =
        kernel_note_info_map_for_rel_paths(&vault_path, &rel_paths, sealed_kernel.inner())?;
    let mut notes = Vec::new();

    for rel_path in rel_paths {
        if let Some(note) = notes_by_id.remove(&rel_path) {
            notes.push(note);
        }
    }

    Ok(notes)
}

/// 只为指定的相对路径列表刷新 kernel-backed Markdown 内容缓存并触发 embedding。
#[tauri::command]
pub fn index_changed_entries(
    vault_path: String,
    paths: Vec<String>,
    app: AppHandle,
    embedding_runtime: State<ai::EmbeddingRuntimeState>,
    sealed_kernel: State<SealedKernelState>,
) -> Result<u32, AppError> {
    let ai_config = read_ai_config(&app)
        .ok()
        .filter(|config| !config.api_key.trim().is_empty());

    sealed_kernel::ensure_vault_open(&vault_path, sealed_kernel.inner())?;
    let refresh_jobs =
        sealed_kernel::prepare_changed_ai_embedding_refresh_jobs(&paths, sealed_kernel.inner())?;

    let indexed_count = refresh_jobs.len() as u32;
    let kernel_session = sealed_kernel::active_session_token(sealed_kernel.inner())?;
    spawn_embedding_tasks(refresh_jobs, ai_config, kernel_session, embedding_runtime);

    Ok(indexed_count)
}

/// 从数据库中删除指定相对路径列表对应的笔记。
#[tauri::command]
pub fn remove_deleted_entries(
    paths: Vec<String>,
    sealed_kernel: State<SealedKernelState>,
) -> Result<u32, AppError> {
    let rel_paths = deleted_markdown_rel_paths(&paths)?;
    let mut removed = 0u32;
    for rel in &rel_paths {
        if sealed_kernel::delete_ai_embedding_note(rel, sealed_kernel.inner())? {
            removed += 1;
        }
    }
    Ok(removed)
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

    #[test]
    fn deleted_markdown_rel_paths_use_kernel_path_filter() {
        let paths = vec![
            " Folder\\Note.md ".to_string(),
            "Folder/Note.md".to_string(),
            "Folder/Note.txt".to_string(),
        ];

        assert_eq!(
            deleted_markdown_rel_paths(&paths).expect("kernel deleted path filter"),
            vec!["Folder/Note.md".to_string()]
        );
    }
}
