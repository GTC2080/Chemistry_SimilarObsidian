use tauri::{AppHandle, State};

use crate::ai;
use crate::db::DbState;
use crate::models::{EnrichedGraphData, GraphData, NoteInfo, TagInfo, TagTreeNode};
use crate::sealed_kernel::{self, SealedKernelState};
use crate::shared::command_utils::read_ai_config;
use crate::AppError;

#[tauri::command]
pub fn search_notes(
    query: String,
    sealed_kernel: State<SealedKernelState>,
) -> Result<Vec<NoteInfo>, AppError> {
    let limit = sealed_kernel::search_note_default_limit()?;
    sealed_kernel::query_search_note_infos(sealed_kernel.inner(), &query, limit)
}

#[tauri::command]
pub fn get_backlinks(
    target_name: String,
    sealed_kernel: State<SealedKernelState>,
) -> Result<Vec<NoteInfo>, AppError> {
    let limit = sealed_kernel::backlink_default_limit()?;
    sealed_kernel::backlink_note_infos(sealed_kernel.inner(), &target_name, limit)
}

#[tauri::command]
pub async fn semantic_search(
    query: String,
    limit: usize,
    app: AppHandle,
    db: State<'_, DbState>,
    embedding_runtime: State<'_, ai::EmbeddingRuntimeState>,
    vector_cache: State<'_, ai::VectorCacheState>,
) -> Result<Vec<NoteInfo>, AppError> {
    let config = read_ai_config(&app)?;
    let query_embedding =
        ai::fetch_embedding_cached(&query, &config, embedding_runtime.inner()).await?;

    let conn = db.conn.lock().map_err(|_| AppError::Lock)?;
    vector_cache.top_k(&query_embedding, limit, None, &conn)
}

#[tauri::command]
pub async fn get_related_notes(
    context_text: String,
    current_note_id: String,
    limit: usize,
    app: AppHandle,
    db: State<'_, DbState>,
    embedding_runtime: State<'_, ai::EmbeddingRuntimeState>,
    vector_cache: State<'_, ai::VectorCacheState>,
) -> Result<Vec<NoteInfo>, AppError> {
    let config = read_ai_config(&app)?;
    let context_embedding =
        ai::fetch_embedding_cached(&context_text, &config, embedding_runtime.inner()).await?;

    let conn = db.conn.lock().map_err(|_| AppError::Lock)?;
    vector_cache.top_k(&context_embedding, limit, Some(&current_note_id), &conn)
}

#[tauri::command]
pub fn get_graph_data(sealed_kernel: State<SealedKernelState>) -> Result<GraphData, AppError> {
    let limit = sealed_kernel::graph_default_limit()?;
    sealed_kernel::query_graph_data(sealed_kernel.inner(), limit)
}

#[tauri::command]
pub fn get_all_tags(sealed_kernel: State<SealedKernelState>) -> Result<Vec<TagInfo>, AppError> {
    let limit = sealed_kernel::tag_catalog_default_limit()?;
    sealed_kernel::query_tag_infos(sealed_kernel.inner(), limit)
}

#[tauri::command]
pub fn get_notes_by_tag(
    tag: String,
    sealed_kernel: State<SealedKernelState>,
) -> Result<Vec<NoteInfo>, AppError> {
    let limit = sealed_kernel::tag_note_default_limit()?;
    sealed_kernel::tag_note_infos(sealed_kernel.inner(), &tag, limit)
}

// ──────────────────────────────────────────
// 性能优化：从前端迁移到 Rust 的命令
// ──────────────────────────────────────────

/// 接收原始笔记内容，在 Rust 端做语义上下文提取 + embedding 搜索
#[tauri::command]
pub async fn get_related_notes_raw(
    raw_content: String,
    current_note_id: String,
    limit: usize,
    app: AppHandle,
    db: State<'_, DbState>,
    embedding_runtime: State<'_, ai::EmbeddingRuntimeState>,
    vector_cache: State<'_, ai::VectorCacheState>,
) -> Result<Vec<NoteInfo>, AppError> {
    let context_text = crate::commands::cmd_compute::build_semantic_context(raw_content);
    if context_text.len() < 24 {
        return Ok(Vec::new());
    }

    let config = read_ai_config(&app)?;
    let context_embedding =
        ai::fetch_embedding_cached(&context_text, &config, embedding_runtime.inner()).await?;

    let conn = db.conn.lock().map_err(|_| AppError::Lock)?;
    vector_cache.top_k(&context_embedding, limit, Some(&current_note_id), &conn)
}

/// 返回预构建的标签树结构（替代前端 buildTagTree）
#[tauri::command]
pub fn get_tag_tree(sealed_kernel: State<SealedKernelState>) -> Result<Vec<TagTreeNode>, AppError> {
    let limit = sealed_kernel::tag_tree_default_limit()?;
    sealed_kernel::query_tag_tree(sealed_kernel.inner(), limit)
}

/// 返回增强版图谱数据，包含预计算的邻接索引（替代前端 useMemo 构建）
#[tauri::command]
pub fn get_enriched_graph_data(
    sealed_kernel: State<SealedKernelState>,
) -> Result<EnrichedGraphData, AppError> {
    let limit = sealed_kernel::graph_default_limit()?;
    sealed_kernel::query_enriched_graph_data(sealed_kernel.inner(), limit)
}
