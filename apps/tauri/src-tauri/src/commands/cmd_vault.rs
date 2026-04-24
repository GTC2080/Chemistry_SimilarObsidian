use std::collections::{HashMap, HashSet};
use std::fs;
use std::path::Path;
use std::sync::Arc;
use std::time::UNIX_EPOCH;

use futures_util::stream::{self, StreamExt};
use tauri::{AppHandle, State};

use crate::ai;
use crate::db::{self, DbState};
use crate::models::NoteInfo;
use crate::sealed_kernel::{self, SealedKernelState};
use crate::shared::command_utils::{
    is_embeddable_extension, parse_ignored_folders, read_ai_config,
};
use crate::watcher::WatcherState;
use crate::AppError;

const KERNEL_NOTE_CATALOG_LIMIT: u64 = 100_000;

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

fn normalize_rel_path(value: &str) -> String {
    value.trim().replace('\\', "/")
}

fn is_markdown_rel_path(value: &str) -> bool {
    Path::new(value)
        .extension()
        .and_then(|ext| ext.to_str())
        .is_some_and(|ext| ext.eq_ignore_ascii_case("md"))
}

fn is_ignored_note(note_id: &str, ignored: &HashSet<String>) -> bool {
    let first = note_id.split('/').next().unwrap_or("");
    ignored.contains(first)
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

fn collect_kernel_note_upserts(
    vault_path: &str,
    ignored: &HashSet<String>,
    existing_timestamps: Option<&HashMap<String, i64>>,
    kernel_state: &SealedKernelState,
) -> Result<Vec<PendingUpsert>, AppError> {
    let notes =
        sealed_kernel::query_note_infos(vault_path, kernel_state, KERNEL_NOTE_CATALOG_LIMIT)?;
    let mut pending_upserts = Vec::new();

    for note in notes {
        if is_ignored_note(&note.id, ignored) || !should_refresh_note(&note, existing_timestamps) {
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

fn note_info_for_changed_markdown(
    vault_path: &str,
    rel_path: &str,
    kernel_state: &SealedKernelState,
) -> Result<Option<(NoteInfo, String)>, AppError> {
    let rel_path = normalize_rel_path(rel_path);
    if rel_path.is_empty() || !is_markdown_rel_path(&rel_path) {
        return Ok(None);
    }

    let content = match sealed_kernel::read_note_by_rel_path(&rel_path, kernel_state) {
        Ok(content) => content,
        Err(_) => return Ok(None),
    };
    let abs = Path::new(vault_path).join(&rel_path);
    let metadata = match fs::metadata(&abs) {
        Ok(metadata) => metadata,
        Err(_) => return Ok(None),
    };
    let (created_at, updated_at) = match extract_timestamps(&metadata) {
        Ok(timestamps) => timestamps,
        Err(_) => return Ok(None),
    };
    let name = Path::new(&rel_path)
        .file_stem()
        .and_then(|s| s.to_str())
        .unwrap_or("未命名")
        .to_string();
    let ext = Path::new(&rel_path)
        .extension()
        .and_then(|s| s.to_str())
        .unwrap_or("")
        .to_ascii_lowercase();

    Ok(Some((
        NoteInfo {
            id: rel_path,
            name,
            path: abs.to_string_lossy().into_owned(),
            created_at,
            updated_at,
            file_extension: ext,
        },
        content,
    )))
}

fn collect_kernel_changed_upserts(
    vault_path: &str,
    paths: &[String],
    existing_timestamps: &HashMap<String, i64>,
    kernel_state: &SealedKernelState,
) -> Result<Vec<PendingUpsert>, AppError> {
    let mut pending_upserts = Vec::new();
    for rel in paths {
        let Some((note, content)) = note_info_for_changed_markdown(vault_path, rel, kernel_state)?
        else {
            continue;
        };
        if !should_refresh_note(&note, Some(existing_timestamps)) || content.trim().is_empty() {
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
        db::upsert_note(
            &conn,
            &upsert.id,
            &upsert.name,
            &upsert.abs_path,
            upsert.created_at,
            upsert.updated_at,
            &upsert.content,
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
        if !is_embeddable_extension(&upsert.ext) {
            continue;
        }
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
    let ignored = parse_ignored_folders(ignored_folders);

    let mut notes = sealed_kernel::query_note_infos(&vault_path, sealed_kernel.inner(), 4096)?;
    notes.retain(|note| {
        let first = note.id.split('/').next().unwrap_or("");
        !ignored.contains(first)
    });
    Ok(notes)
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
    let ignored = parse_ignored_folders(ignored_folders);
    let ai_config = read_ai_config(&app)
        .ok()
        .filter(|config| !config.api_key.trim().is_empty());
    let existing_timestamps = {
        let conn = db.conn.lock().map_err(|_| AppError::Lock)?;
        db::get_all_note_timestamps(&conn)?
    };

    let pending_upserts = collect_kernel_note_upserts(
        &vault_path,
        &ignored,
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

/// Extract created_at / updated_at timestamps from file metadata.
fn extract_timestamps(metadata: &fs::Metadata) -> Result<(i64, i64), AppError> {
    let updated_at = metadata
        .modified()?
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_secs() as i64;
    let created_at = metadata
        .created()
        .map(|t| t.duration_since(UNIX_EPOCH).unwrap_or_default().as_secs() as i64)
        .unwrap_or(updated_at);
    Ok((created_at, updated_at))
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
    let ignored = HashSet::new();
    let pending_upserts =
        collect_kernel_note_upserts(&vault_path, &ignored, None, sealed_kernel.inner())?;
    let all_notes: Vec<_> = pending_upserts
        .iter()
        .map(|note| (note.id.clone(), note.abs_path.clone(), note.content.clone()))
        .collect();

    // Lock optimization: refresh the compatibility cache and clear embeddings in one lock.
    {
        let conn = db.conn.lock().map_err(|_| AppError::Lock)?;
        db::clear_all_embeddings(&conn)?;
        conn.execute_batch("BEGIN")?;
        for upsert in &pending_upserts {
            db::upsert_note(
                &conn,
                &upsert.id,
                &upsert.name,
                &upsert.abs_path,
                upsert.created_at,
                upsert.updated_at,
                &upsert.content,
            )?;
        }
        conn.execute_batch("COMMIT")?;
    }

    // Process embeddings concurrently with buffer_unordered(4)
    let results: Vec<_> = stream::iter(all_notes)
        .filter_map(|(id, absolute_path, content)| {
            let ext = Path::new(&absolute_path)
                .extension()
                .and_then(|s| s.to_str())
                .unwrap_or("")
                .to_ascii_lowercase();
            if !is_embeddable_extension(&ext) || content.trim().is_empty() {
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
    let mut notes: Vec<NoteInfo> = Vec::new();

    for rel in paths {
        if let Some((note, _content)) =
            note_info_for_changed_markdown(&vault_path, &rel, sealed_kernel.inner())?
        {
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
        db::get_all_note_timestamps(&conn)?
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
    let conn = db.conn.lock().map_err(|_| AppError::Lock)?;
    let mut removed = 0u32;
    for rel in &paths {
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
) -> Result<(), AppError> {
    let ignored = parse_ignored_folders(ignored_folders);
    watcher
        .start(&vault_path, &ignored, app)
        .map_err(AppError::Custom)
}

/// 停止文件系统监听。切换 vault 或关闭时调用。
#[tauri::command]
pub fn stop_watcher(watcher: State<WatcherState>) -> Result<(), AppError> {
    watcher.stop();
    Ok(())
}
