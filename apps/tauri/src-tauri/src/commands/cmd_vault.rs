use std::collections::HashSet;
use std::fs;
use std::path::Path;
use std::sync::Arc;
use std::time::UNIX_EPOCH;

use futures_util::stream::{self, StreamExt};
use tauri::{AppHandle, State};
use walkdir::WalkDir;

use crate::ai;
use crate::db::{self, DbState};
use crate::models::NoteInfo;
use crate::sealed_kernel::{self, SealedKernelState};
use crate::shared::command_utils::{
    extract_pdf_text, is_embeddable_extension, is_mol_extension, is_molecular_extension,
    is_paper_extension, is_pdf_extension, is_spectroscopy_extension, is_supported_extension,
    is_text_extension, read_ai_config,
};
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

fn parse_ignored_folders(ignored_folders: Option<String>) -> HashSet<String> {
    ignored_folders
        .unwrap_or_default()
        .split(',')
        .map(|s| s.trim().trim_matches('/').trim_matches('\\').to_string())
        .filter(|s| !s.is_empty())
        .collect()
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

// ---------------------------------------------------------------------------
// scan_vault — fast metadata-only walk (Phase 1)
// ---------------------------------------------------------------------------

/// Walk the vault directory and return file metadata immediately.
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

/// Incrementally read text content for changed files, upsert to DB, and
/// spawn embedding tasks. Designed to run in the background after
/// `scan_vault` has already returned the file list.
#[tauri::command]
pub fn index_vault_content(
    vault_path: String,
    ignored_folders: Option<String>,
    app: AppHandle,
    db: State<DbState>,
    embedding_runtime: State<ai::EmbeddingRuntimeState>,
    vector_cache: State<ai::VectorCacheState>,
) -> Result<u32, AppError> {
    let vault = Path::new(&vault_path);
    if !vault.is_dir() {
        return Ok(0);
    }
    let ignored = parse_ignored_folders(ignored_folders);
    let ai_config = read_ai_config(&app)
        .ok()
        .filter(|config| !config.api_key.trim().is_empty());

    // Read existing timestamps once
    let existing_timestamps = {
        let conn = db.conn.lock().map_err(|_| AppError::Lock)?;
        db::get_all_note_timestamps(&conn)?
    };

    let mut pending_upserts: Vec<PendingUpsert> = Vec::new();

    for entry in walk_vault(vault, &ignored) {
        let entry = match entry {
            Ok(e) => e,
            Err(_) => continue,
        };
        if !entry.file_type().is_file() {
            continue;
        }
        let path = entry.path();
        let ext = path.extension().and_then(|e| e.to_str()).unwrap_or("");

        // Only index text and PDF files (skip images, molecular, spectroscopy, etc.)
        let should_index = (is_text_extension(ext)
            && !is_mol_extension(ext)
            && !is_spectroscopy_extension(ext)
            && !is_molecular_extension(ext)
            && !is_paper_extension(ext))
            || is_pdf_extension(ext);
        if !should_index {
            continue;
        }

        let metadata = match fs::metadata(path) {
            Ok(m) => m,
            Err(_) => continue,
        };
        let (created_at, updated_at) = match extract_timestamps(&metadata) {
            Ok(ts) => ts,
            Err(_) => continue,
        };

        let relative_path = path.strip_prefix(vault).unwrap_or(path);
        let id = relative_path.to_string_lossy().into_owned();

        let needs_update = match existing_timestamps.get(&id) {
            None => true,
            Some(&db_ts) => updated_at > db_ts,
        };
        if !needs_update {
            continue;
        }

        let content = if is_pdf_extension(ext) {
            match extract_pdf_text(path) {
                Ok(text) => text,
                Err(e) => {
                    eprintln!("[PDF提取] {}", e);
                    continue;
                }
            }
        } else {
            match fs::read_to_string(path) {
                Ok(text) => text,
                Err(_) => continue,
            }
        };

        if content.trim().is_empty() {
            continue;
        }

        let name = path
            .file_stem()
            .and_then(|s| s.to_str())
            .unwrap_or("未命名")
            .to_string();

        pending_upserts.push(PendingUpsert {
            id,
            name,
            abs_path: path.to_string_lossy().into_owned(),
            created_at,
            updated_at,
            content,
            ext: ext.to_lowercase(),
        });
    }

    let indexed_count = pending_upserts.len() as u32;

    // Batch upsert
    if !pending_upserts.is_empty() {
        let conn = db.conn.lock().map_err(|_| AppError::Lock)?;
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

    // Spawn embedding tasks with version-based dedup
    for upsert in pending_upserts {
        if is_embeddable_extension(&upsert.ext) {
            if let Some(config) = ai_config.clone() {
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
                    file_extension: upsert.ext.clone(),
                };
                let version = embedding_runtime.bump_version(&note_id);

                tauri::async_runtime::spawn(async move {
                    match ai::fetch_embedding_cached(
                        &text_for_embedding,
                        &config,
                        &embedding_runtime,
                    )
                    .await
                    {
                        Ok(embedding) => {
                            // Only write if this is still the latest version
                            if !embedding_runtime.is_current_version(&note_id, version) {
                                eprintln!("[向量化] 跳过过期结果 [{}] v{}", note_id, version);
                                return;
                            }
                            if let Ok(conn) = db_conn.lock() {
                                if let Err(e) =
                                    db::update_note_embedding(&conn, &note_id, &embedding)
                                {
                                    eprintln!("[向量化] 写入失败 [{}]: {}", note_id, e);
                                } else {
                                    vector_cache.upsert(note_info, embedding.clone());
                                    eprintln!(
                                        "[向量化] 成功 [{}]: {}维向量",
                                        note_id,
                                        embedding.len()
                                    );
                                }
                            }
                        }
                        Err(e) => eprintln!("[向量化] 跳过 [{}]: {}", note_id, e),
                    }
                });
            }
        }
    }

    Ok(indexed_count)
}

// ---------------------------------------------------------------------------
// Internal helpers for vault walking
// ---------------------------------------------------------------------------

/// Create a filtered WalkDir iterator for the vault.
fn walk_vault<'a>(
    vault: &'a Path,
    ignored: &'a HashSet<String>,
) -> impl Iterator<Item = walkdir::Result<walkdir::DirEntry>> + 'a {
    WalkDir::new(vault)
        .follow_links(true)
        .into_iter()
        .filter_entry(move |e| {
            if e.depth() == 0 {
                return true;
            }
            let name = match e.file_name().to_str() {
                Some(n) => n,
                None => return false,
            };
            !name.starts_with('.') && !ignored.contains(name)
        })
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
) -> Result<u32, AppError> {
    // 清空内存缓存，重建完成后下次查询会重新加载
    vector_cache.clear();
    let config = read_ai_config(&app)?;
    if config.api_key.trim().is_empty() {
        return Err(AppError::Custom(
            "未配置 AI API Key，无法重建向量索引".to_string(),
        ));
    }

    // Lock optimization: merge get_all_notes + clear_all_embeddings into single lock
    let all_notes = {
        let conn = db.conn.lock().map_err(|_| AppError::Lock)?;
        let notes = db::get_all_notes_for_embedding(&conn)?;
        db::clear_all_embeddings(&conn)?;
        notes
    };

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
) -> Result<Vec<NoteInfo>, AppError> {
    let vault = Path::new(&vault_path);
    let mut notes: Vec<NoteInfo> = Vec::new();

    for rel in &paths {
        let abs = vault.join(rel);
        if !abs.is_file() {
            continue;
        }
        let ext = abs.extension().and_then(|e| e.to_str()).unwrap_or("");
        if !is_supported_extension(ext) {
            continue;
        }
        let metadata = match fs::metadata(&abs) {
            Ok(m) => m,
            Err(_) => continue,
        };
        let (created_at, updated_at) = match extract_timestamps(&metadata) {
            Ok(ts) => ts,
            Err(_) => continue,
        };
        let name = abs
            .file_stem()
            .and_then(|s| s.to_str())
            .unwrap_or("未命名")
            .to_string();

        notes.push(NoteInfo {
            id: rel.clone(),
            name,
            path: abs.to_string_lossy().into_owned(),
            created_at,
            updated_at,
            file_extension: ext.to_lowercase(),
        });
    }

    Ok(notes)
}

/// 只为指定的相对路径列表做内容索引（读文本 / PDF 抽取 + DB upsert + embedding）。
#[tauri::command]
pub fn index_changed_entries(
    vault_path: String,
    paths: Vec<String>,
    app: AppHandle,
    db: State<DbState>,
    embedding_runtime: State<ai::EmbeddingRuntimeState>,
    vector_cache: State<ai::VectorCacheState>,
) -> Result<u32, AppError> {
    let vault = Path::new(&vault_path);
    let ai_config = read_ai_config(&app)
        .ok()
        .filter(|config| !config.api_key.trim().is_empty());

    let existing_timestamps = {
        let conn = db.conn.lock().map_err(|_| AppError::Lock)?;
        db::get_all_note_timestamps(&conn)?
    };

    let mut pending_upserts: Vec<PendingUpsert> = Vec::new();

    for rel in &paths {
        let abs = vault.join(rel);
        if !abs.is_file() {
            continue;
        }
        let ext = abs.extension().and_then(|e| e.to_str()).unwrap_or("");

        let should_index = (is_text_extension(ext)
            && !is_mol_extension(ext)
            && !is_spectroscopy_extension(ext)
            && !is_molecular_extension(ext)
            && !is_paper_extension(ext))
            || is_pdf_extension(ext);
        if !should_index {
            continue;
        }

        let metadata = match fs::metadata(&abs) {
            Ok(m) => m,
            Err(_) => continue,
        };
        let (created_at, updated_at) = match extract_timestamps(&metadata) {
            Ok(ts) => ts,
            Err(_) => continue,
        };

        let id = rel.clone();

        let needs_update = match existing_timestamps.get(&id) {
            None => true,
            Some(&db_ts) => updated_at > db_ts,
        };
        if !needs_update {
            continue;
        }

        let content = if is_pdf_extension(ext) {
            match extract_pdf_text(&abs) {
                Ok(text) => text,
                Err(e) => {
                    eprintln!("[PDF提取] {}", e);
                    continue;
                }
            }
        } else {
            match fs::read_to_string(&abs) {
                Ok(text) => text,
                Err(_) => continue,
            }
        };

        if content.trim().is_empty() {
            continue;
        }

        let name = abs
            .file_stem()
            .and_then(|s| s.to_str())
            .unwrap_or("未命名")
            .to_string();

        pending_upserts.push(PendingUpsert {
            id,
            name,
            abs_path: abs.to_string_lossy().into_owned(),
            created_at,
            updated_at,
            content,
            ext: ext.to_lowercase(),
        });
    }

    let indexed_count = pending_upserts.len() as u32;

    if !pending_upserts.is_empty() {
        let conn = db.conn.lock().map_err(|_| AppError::Lock)?;
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

    for upsert in pending_upserts {
        if is_embeddable_extension(&upsert.ext) {
            if let Some(config) = ai_config.clone() {
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
                    file_extension: upsert.ext.clone(),
                };
                let version = embedding_runtime.bump_version(&note_id);

                tauri::async_runtime::spawn(async move {
                    match ai::fetch_embedding_cached(
                        &text_for_embedding,
                        &config,
                        &embedding_runtime,
                    )
                    .await
                    {
                        Ok(embedding) => {
                            if !embedding_runtime.is_current_version(&note_id, version) {
                                return;
                            }
                            if let Ok(conn) = db_conn.lock() {
                                if let Err(e) =
                                    db::update_note_embedding(&conn, &note_id, &embedding)
                                {
                                    eprintln!("[向量化] 写入失败 [{}]: {}", note_id, e);
                                } else {
                                    vector_cache.upsert(note_info, embedding.clone());
                                }
                            }
                        }
                        Err(e) => eprintln!("[向量化] 跳过 [{}]: {}", note_id, e),
                    }
                });
            }
        }
    }

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
