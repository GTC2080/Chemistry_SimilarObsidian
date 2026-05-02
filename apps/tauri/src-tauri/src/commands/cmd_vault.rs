use std::collections::{HashMap, HashSet};
use std::path::Path;
use std::sync::Arc;

use futures_util::stream::{self, StreamExt};
use tauri::{AppHandle, State};

use crate::ai;
use crate::db::{self, DbState};
use crate::models::NoteInfo;
use crate::sealed_kernel::{self, SealedKernelState};
use crate::shared::command_utils::read_ai_config;
use crate::watcher::WatcherState;
use crate::AppError;

#[tauri::command]
pub fn init_vault(
    vault_path: String,
    db: State<DbState>,
    vector_cache: State<ai::VectorCacheState>,
    sealed_kernel: State<SealedKernelState>,
) -> Result<(), AppError> {
    sealed_kernel::ensure_vault_open(&vault_path, sealed_kernel.inner())?;
    let new_conn = db::init_db(&vault_path)?;
    let mut conn = db.conn.lock().map_err(|_| AppError::Lock)?;
    *conn = new_conn;
    vector_cache.clear();
    Ok(())
}

/// Holds data for a note that needs to be upserted into the database.
struct PendingUpsert {
    id: String,
    name: String,
    abs_path: String,
    created_at: i64,
    updated_at: i64,
    content: String,
    ext: String,
}

fn should_refresh_note(
    note: &NoteInfo,
    existing_timestamps: Option<&HashMap<String, i64>>,
) -> bool {
    existing_timestamps
        .and_then(|timestamps| timestamps.get(&note.id))
        .is_none_or(|db_ts| note.updated_at > *db_ts)
}

fn pending_upsert_from_note(note: NoteInfo, content: String) -> PendingUpsert {
    PendingUpsert {
        id: note.id,
        name: note.name,
        abs_path: note.path,
        created_at: note.created_at,
        updated_at: note.updated_at,
        content,
        ext: note.file_extension,
    }
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

fn collect_kernel_note_upserts(
    vault_path: &str,
    ignored_roots: &str,
    existing_timestamps: Option<&HashMap<String, i64>>,
    kernel_state: &SealedKernelState,
) -> Result<Vec<PendingUpsert>, AppError> {
    let notes = sealed_kernel::query_note_infos_filtered(
        vault_path,
        kernel_state,
        sealed_kernel::note_catalog_default_limit()?,
        ignored_roots,
    )?;
    let mut pending_upserts = Vec::new();

    for note in notes {
        if !should_refresh_note(&note, existing_timestamps) {
            continue;
        }
        let content = match sealed_kernel::read_note_by_rel_path(&note.id, kernel_state) {
            Ok(content) => content,
            Err(err) => {
                eprintln!("[kernel-index] 跳过 [{}]: {}", note.id, err);
                continue;
            }
        };
        if content.trim().is_empty() {
            continue;
        }
        pending_upserts.push(pending_upsert_from_note(note, content));
    }

    Ok(pending_upserts)
}

fn collect_kernel_changed_upserts(
    vault_path: &str,
    paths: &[String],
    existing_timestamps: &HashMap<String, i64>,
    kernel_state: &SealedKernelState,
) -> Result<Vec<PendingUpsert>, AppError> {
    let rel_paths = sealed_kernel::filter_changed_markdown_paths(paths)?;
    let mut notes_by_id = kernel_note_info_map_for_rel_paths(vault_path, &rel_paths, kernel_state)?;
    let mut pending_upserts = Vec::new();

    for rel_path in rel_paths {
        let Some(note) = notes_by_id.remove(&rel_path) else {
            continue;
        };
        if !should_refresh_note(&note, Some(existing_timestamps)) {
            continue;
        }
        let content = match sealed_kernel::read_note_by_rel_path(&note.id, kernel_state) {
            Ok(content) => content,
            Err(err) => {
                eprintln!("[kernel-index] 跳过 [{}]: {}", note.id, err);
                continue;
            }
        };
        if content.trim().is_empty() {
            continue;
        }
        pending_upserts.push(pending_upsert_from_note(note, content));
    }
    Ok(pending_upserts)
}

fn write_note_cache(db: &DbState, pending_upserts: &[PendingUpsert]) -> Result<(), AppError> {
    if pending_upserts.is_empty() {
        return Ok(());
    }

    let conn = db.conn.lock().map_err(|_| AppError::Lock)?;
    conn.execute_batch("BEGIN")?;
    for upsert in pending_upserts {
        db::upsert_embedding_note_metadata(
            &conn,
            &upsert.id,
            &upsert.name,
            &upsert.abs_path,
            upsert.created_at,
            upsert.updated_at,
        )?;
    }
    conn.execute_batch("COMMIT")?;
    Ok(())
}

fn spawn_embedding_tasks(
    pending_upserts: Vec<PendingUpsert>,
    ai_config: Option<ai::AiConfig>,
    db: State<DbState>,
    embedding_runtime: State<ai::EmbeddingRuntimeState>,
    vector_cache: State<ai::VectorCacheState>,
) {
    let Some(config) = ai_config else {
        return;
    };

    for upsert in pending_upserts {
        let db_conn = Arc::clone(&db.conn);
        let embedding_runtime = embedding_runtime.inner().clone();
        let vector_cache = vector_cache.inner().clone();
        let note_id = upsert.id;
        let text_for_embedding = upsert.content;
        let note_info = NoteInfo {
            id: note_id.clone(),
            name: upsert.name,
            path: upsert.abs_path,
            created_at: upsert.created_at,
            updated_at: upsert.updated_at,
            file_extension: upsert.ext,
        };
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
                    if let Ok(conn) = db_conn.lock() {
                        if let Err(e) = db::update_note_embedding(&conn, &note_id, &embedding) {
                            eprintln!("[向量化] 写入失败 [{}]: {}", note_id, e);
                        } else {
                            vector_cache.upsert(note_info, embedding.clone());
                            eprintln!("[向量化] 成功 [{}]: {}维向量", note_id, embedding.len());
                        }
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
    if !Path::new(&vault_path).is_dir() {
        return Err(AppError::Custom(format!(
            "路径不存在或不是一个有效目录: {}",
            vault_path
        )));
    }
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

/// Refresh the AI compatibility cache from the kernel note catalog and spawn
/// embedding tasks. The kernel is the content source; the Rust DB only stores
/// legacy embedding rows until semantic retrieval moves behind the kernel too.
#[tauri::command]
pub fn index_vault_content(
    vault_path: String,
    ignored_folders: Option<String>,
    app: AppHandle,
    db: State<DbState>,
    embedding_runtime: State<ai::EmbeddingRuntimeState>,
    vector_cache: State<ai::VectorCacheState>,
    sealed_kernel: State<SealedKernelState>,
) -> Result<u32, AppError> {
    let ai_config = read_ai_config(&app)
        .ok()
        .filter(|config| !config.api_key.trim().is_empty());
    let existing_timestamps = {
        let conn = db.conn.lock().map_err(|_| AppError::Lock)?;
        db::get_embedding_note_timestamps(&conn)?
    };

    let pending_upserts = collect_kernel_note_upserts(
        &vault_path,
        ignored_folders.as_deref().unwrap_or(""),
        Some(&existing_timestamps),
        sealed_kernel.inner(),
    )?;

    let indexed_count = pending_upserts.len() as u32;
    write_note_cache(db.inner(), &pending_upserts)?;
    spawn_embedding_tasks(
        pending_upserts,
        ai_config,
        db,
        embedding_runtime,
        vector_cache,
    );

    Ok(indexed_count)
}

#[tauri::command]
pub async fn rebuild_vector_index(
    app: AppHandle,
    db: State<'_, DbState>,
    embedding_runtime: State<'_, ai::EmbeddingRuntimeState>,
    vector_cache: State<'_, ai::VectorCacheState>,
    sealed_kernel: State<'_, SealedKernelState>,
) -> Result<u32, AppError> {
    // 清空内存缓存，重建完成后下次查询会重新加载
    vector_cache.clear();
    let config = read_ai_config(&app)?;
    if config.api_key.trim().is_empty() {
        return Err(AppError::Custom(
            "未配置 AI API Key，无法重建向量索引".to_string(),
        ));
    }

    let vault_path = sealed_kernel::active_vault_path(sealed_kernel.inner())?;
    let pending_upserts =
        collect_kernel_note_upserts(&vault_path, "", None, sealed_kernel.inner())?;
    let all_notes: Vec<_> = pending_upserts
        .iter()
        .map(|note| (note.id.clone(), note.content.clone()))
        .collect();

    // Lock optimization: refresh the compatibility cache and clear embeddings in one lock.
    {
        let conn = db.conn.lock().map_err(|_| AppError::Lock)?;
        db::clear_all_embeddings(&conn)?;
        conn.execute_batch("BEGIN")?;
        for upsert in &pending_upserts {
            db::upsert_embedding_note_metadata(
                &conn,
                &upsert.id,
                &upsert.name,
                &upsert.abs_path,
                upsert.created_at,
                upsert.updated_at,
            )?;
        }
        conn.execute_batch("COMMIT")?;
    }

    // Process embeddings concurrently with buffer_unordered(4)
    let results: Vec<_> = stream::iter(all_notes)
        .filter_map(|(id, content)| {
            if content.trim().is_empty() {
                return std::future::ready(None);
            }
            std::future::ready(Some((id, content)))
        })
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

    // Batch write all embeddings in one lock + transaction
    let conn = db.conn.lock().map_err(|_| AppError::Lock)?;
    conn.execute_batch("BEGIN")?;
    for (id, embedding) in &results {
        db::update_note_embedding(&conn, id, embedding)?;
    }
    conn.execute_batch("COMMIT")?;

    Ok(results.len() as u32)
}

#[tauri::command]
pub async fn write_note(
    vault_path: String,
    file_path: String,
    content: String,
    vector_cache: State<'_, ai::VectorCacheState>,
    sealed_kernel: State<'_, SealedKernelState>,
) -> Result<(), AppError> {
    sealed_kernel::write_note_by_file_path(
        &vault_path,
        &file_path,
        &content,
        sealed_kernel.inner(),
    )?;
    vector_cache.clear();
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
    db: State<DbState>,
    embedding_runtime: State<ai::EmbeddingRuntimeState>,
    vector_cache: State<ai::VectorCacheState>,
    sealed_kernel: State<SealedKernelState>,
) -> Result<u32, AppError> {
    let ai_config = read_ai_config(&app)
        .ok()
        .filter(|config| !config.api_key.trim().is_empty());

    let existing_timestamps = {
        let conn = db.conn.lock().map_err(|_| AppError::Lock)?;
        db::get_embedding_note_timestamps(&conn)?
    };

    let pending_upserts = collect_kernel_changed_upserts(
        &vault_path,
        &paths,
        &existing_timestamps,
        sealed_kernel.inner(),
    )?;

    let indexed_count = pending_upserts.len() as u32;
    write_note_cache(db.inner(), &pending_upserts)?;
    spawn_embedding_tasks(
        pending_upserts,
        ai_config,
        db,
        embedding_runtime,
        vector_cache,
    );

    Ok(indexed_count)
}

/// 从数据库中删除指定相对路径列表对应的笔记。
#[tauri::command]
pub fn remove_deleted_entries(
    paths: Vec<String>,
    db: State<DbState>,
    vector_cache: State<ai::VectorCacheState>,
) -> Result<u32, AppError> {
    let rel_paths = deleted_markdown_rel_paths(&paths)?;
    let conn = db.conn.lock().map_err(|_| AppError::Lock)?;
    let mut removed = 0u32;
    for rel in &rel_paths {
        if db::delete_note_by_id(&conn, rel).is_ok() {
            vector_cache.remove(rel);
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
