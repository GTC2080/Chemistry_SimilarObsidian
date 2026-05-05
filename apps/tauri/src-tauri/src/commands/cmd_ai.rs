use tauri::{AppHandle, State};

use crate::ai;
use crate::sealed_kernel::{self, SealedKernelState};
use crate::shared::command_utils::read_ai_config;
use crate::AppError;

#[tauri::command]
pub async fn test_ai_connection(
    app: AppHandle,
    embedding_runtime: State<'_, ai::EmbeddingRuntimeState>,
) -> Result<String, AppError> {
    let config = read_ai_config(&app)?;
    let embedding =
        ai::fetch_embedding_cached("测试连接", &config, embedding_runtime.inner()).await?;
    Ok(format!("连接成功，返回 {} 维向量", embedding.len()))
}

#[tauri::command]
pub async fn ponder_node(
    topic: String,
    context: String,
    app: AppHandle,
) -> Result<String, AppError> {
    let config = read_ai_config(&app)?;
    ai::ponder_node(&topic, &context, &config)
        .await
        .map_err(Into::into)
}

#[tauri::command]
pub async fn ask_vault(
    question: String,
    active_note_id: Option<String>,
    on_event: tauri::ipc::Channel<String>,
    app: AppHandle,
    embedding_runtime: State<'_, ai::EmbeddingRuntimeState>,
    sealed_kernel: State<'_, SealedKernelState>,
) -> Result<(), AppError> {
    let config = read_ai_config(&app)?;
    let query_embedding =
        ai::fetch_embedding_cached(&question, &config, embedding_runtime.inner()).await?;
    let top_notes = sealed_kernel::query_ai_embedding_top_note_infos(
        &query_embedding,
        sealed_kernel::ai_rag_top_note_limit()? as u64,
        active_note_id.as_deref(),
        sealed_kernel.inner(),
    )?;

    let note_ids = active_note_id
        .iter()
        .cloned()
        .chain(top_notes.iter().map(|note| note.id.clone()));
    let context =
        sealed_kernel::build_ai_rag_context_from_note_ids(note_ids, sealed_kernel.inner())?;
    ai::stream_chat_with_context(&question, &context, &config, |chunk| {
        on_event
            .send(chunk)
            .map_err(|e| format!("发送 IPC 消息失败: {}", e))
    })
    .await?;

    Ok(())
}
