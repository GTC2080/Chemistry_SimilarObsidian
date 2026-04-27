use tauri::{AppHandle, State};

use crate::ai;
use crate::db::DbState;
use crate::sealed_kernel::{self, SealedKernelState};
use crate::shared::command_utils::read_ai_config;
use crate::AppError;

fn note_display_name(note_id: &str) -> String {
    std::path::Path::new(note_id)
        .file_stem()
        .and_then(|value| value.to_str())
        .filter(|value| !value.is_empty())
        .unwrap_or(note_id)
        .to_string()
}

fn collect_rag_note_contents(
    note_ids: impl IntoIterator<Item = String>,
    kernel_state: &SealedKernelState,
) -> Result<Vec<(String, String)>, AppError> {
    let mut contents = Vec::new();

    for rel_path in rag_note_rel_paths(note_ids)? {
        match sealed_kernel::read_note_by_rel_path(&rel_path, kernel_state) {
            Ok(content) if !content.trim().is_empty() => {
                contents.push((note_display_name(&rel_path), content));
            }
            Ok(_) => {}
            Err(err) => eprintln!("[ask_vault] 跳过 RAG 笔记 [{}]: {}", rel_path, err),
        }
    }

    Ok(contents)
}

fn rag_note_rel_paths(note_ids: impl IntoIterator<Item = String>) -> Result<Vec<String>, AppError> {
    let candidates: Vec<String> = note_ids.into_iter().collect();
    sealed_kernel::filter_changed_markdown_paths(&candidates)
}

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
    db: State<'_, DbState>,
    embedding_runtime: State<'_, ai::EmbeddingRuntimeState>,
    vector_cache: State<'_, ai::VectorCacheState>,
    sealed_kernel: State<'_, SealedKernelState>,
) -> Result<(), AppError> {
    let config = read_ai_config(&app)?;
    let query_embedding =
        ai::fetch_embedding_cached(&question, &config, embedding_runtime.inner()).await?;
    sealed_kernel::active_vault_path(sealed_kernel.inner())?;

    let top_notes = {
        let conn = db.conn.lock().map_err(|_| AppError::Lock)?;
        vector_cache.top_k(&query_embedding, 5, active_note_id.as_deref(), &conn)?
    };

    let related_ids = top_notes.iter().map(|note| note.id.clone());
    let note_contents = collect_rag_note_contents(
        active_note_id.iter().cloned().chain(related_ids),
        sealed_kernel.inner(),
    )?;

    let context = ai::build_rag_context(&note_contents)?;
    ai::stream_chat_with_context(&question, &context, &config, |chunk| {
        on_event
            .send(chunk)
            .map_err(|e| format!("发送 IPC 消息失败: {}", e))
    })
    .await?;

    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn rag_note_rel_paths_delegate_normalization_and_dedup_to_kernel() {
        let paths = vec![
            " Folder\\Note.md ".to_string(),
            "Folder/Note.md".to_string(),
            "Folder/Note.txt".to_string(),
            "Other.MD".to_string(),
            "".to_string(),
        ];

        assert_eq!(
            rag_note_rel_paths(paths).expect("kernel RAG path filter"),
            vec!["Folder/Note.md".to_string(), "Other.MD".to_string()]
        );
    }
}
