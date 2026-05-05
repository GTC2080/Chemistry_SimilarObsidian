use tauri::{AppHandle, State};

use crate::ai;
use crate::models::{EnrichedGraphData, GraphData, NoteInfo, TagInfo, TagTreeNode};
use crate::sealed_kernel::{self, SealedKernelState};
use crate::shared::command_utils::read_ai_config;
use crate::AppError;

async fn query_notes_by_embedding(
    query_text: &str,
    excluded_note_id: Option<&str>,
    limit: usize,
    app: &AppHandle,
    embedding_runtime: &ai::EmbeddingRuntimeState,
    sealed_kernel: &SealedKernelState,
) -> Result<Vec<NoteInfo>, AppError> {
    if limit == 0 {
        return Ok(Vec::new());
    }

    let config = read_ai_config(app)?;
    let query_embedding =
        ai::fetch_embedding_cached(query_text, &config, embedding_runtime).await?;
    sealed_kernel::query_ai_embedding_top_note_infos(
        &query_embedding,
        limit as u64,
        excluded_note_id,
        sealed_kernel,
    )
}

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
    embedding_runtime: State<'_, ai::EmbeddingRuntimeState>,
    sealed_kernel: State<'_, SealedKernelState>,
) -> Result<Vec<NoteInfo>, AppError> {
    query_notes_by_embedding(
        &query,
        None,
        limit,
        &app,
        embedding_runtime.inner(),
        sealed_kernel.inner(),
    )
    .await
}

#[tauri::command]
pub async fn get_related_notes(
    context_text: String,
    current_note_id: String,
    limit: usize,
    app: AppHandle,
    embedding_runtime: State<'_, ai::EmbeddingRuntimeState>,
    sealed_kernel: State<'_, SealedKernelState>,
) -> Result<Vec<NoteInfo>, AppError> {
    query_notes_by_embedding(
        &context_text,
        Some(&current_note_id),
        limit,
        &app,
        embedding_runtime.inner(),
        sealed_kernel.inner(),
    )
    .await
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
// Host commands backed by kernel-owned search and context rules.
// ──────────────────────────────────────────

/// 接收原始笔记内容，通过 kernel 构造语义上下文，再由 host 侧触发 embedding 搜索。
#[tauri::command]
pub async fn get_related_notes_raw(
    raw_content: String,
    current_note_id: String,
    limit: usize,
    app: AppHandle,
    embedding_runtime: State<'_, ai::EmbeddingRuntimeState>,
    sealed_kernel: State<'_, SealedKernelState>,
) -> Result<Vec<NoteInfo>, AppError> {
    if limit == 0 {
        return Ok(Vec::new());
    }
    let context_text = crate::commands::cmd_compute::build_semantic_context(raw_content);
    let min_context_bytes = sealed_kernel::semantic_context_min_bytes()?;
    if context_text.len() < min_context_bytes {
        return Ok(Vec::new());
    }

    query_notes_by_embedding(
        &context_text,
        Some(&current_note_id),
        limit,
        &app,
        embedding_runtime.inner(),
        sealed_kernel.inner(),
    )
    .await
}

/// 返回 kernel 预构建的标签树结构（替代前端 buildTagTree）。
#[tauri::command]
pub fn get_tag_tree(sealed_kernel: State<SealedKernelState>) -> Result<Vec<TagTreeNode>, AppError> {
    let limit = sealed_kernel::tag_tree_default_limit()?;
    sealed_kernel::query_tag_tree(sealed_kernel.inner(), limit)
}

/// 返回 kernel 增强版图谱数据，包含预计算的邻接索引（替代前端 useMemo 构建）。
#[tauri::command]
pub fn get_enriched_graph_data(
    sealed_kernel: State<SealedKernelState>,
) -> Result<EnrichedGraphData, AppError> {
    let limit = sealed_kernel::graph_default_limit()?;
    sealed_kernel::query_enriched_graph_data(sealed_kernel.inner(), limit)
}
