use futures_util::stream::{self, StreamExt};
use tauri::{AppHandle, State};

use crate::ai;
use crate::sealed_kernel::{self, SealedKernelState};
use crate::shared::command_utils::read_ai_config;
use crate::AppError;

fn spawn_embedding_tasks(
    refresh_jobs: Vec<sealed_kernel::AiEmbeddingRefreshJob>,
    ai_config: Option<ai::AiConfig>,
    kernel_session: usize,
    embedding_runtime: &ai::EmbeddingRuntimeState,
) {
    let Some(config) = ai_config else {
        return;
    };

    for job in refresh_jobs {
        let embedding_runtime = embedding_runtime.clone();
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

fn optional_embedding_config(app: &AppHandle) -> Option<ai::AiConfig> {
    read_ai_config(app)
        .ok()
        .filter(|config| !config.api_key.trim().is_empty())
}

fn require_embedding_config(app: &AppHandle) -> Result<ai::AiConfig, AppError> {
    let config = read_ai_config(app)?;
    if config.api_key.trim().is_empty() {
        return Err(AppError::Custom(
            "未配置 AI API Key，无法重建向量索引".to_string(),
        ));
    }
    Ok(config)
}

fn spawn_prepared_embedding_jobs(
    refresh_jobs: Vec<sealed_kernel::AiEmbeddingRefreshJob>,
    app: &AppHandle,
    embedding_runtime: &ai::EmbeddingRuntimeState,
    sealed_kernel: &SealedKernelState,
) -> Result<u32, AppError> {
    let indexed_count = refresh_jobs.len() as u32;
    let kernel_session = sealed_kernel::active_session_token(sealed_kernel)?;
    spawn_embedding_tasks(
        refresh_jobs,
        optional_embedding_config(app),
        kernel_session,
        embedding_runtime,
    );
    Ok(indexed_count)
}

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
    sealed_kernel::ensure_vault_open(&vault_path, sealed_kernel.inner())?;
    let refresh_jobs = sealed_kernel::prepare_ai_embedding_refresh_jobs(
        ignored_folders.as_deref().unwrap_or(""),
        false,
        sealed_kernel.inner(),
    )?;

    spawn_prepared_embedding_jobs(
        refresh_jobs,
        &app,
        embedding_runtime.inner(),
        sealed_kernel.inner(),
    )
}

#[tauri::command]
pub async fn rebuild_vector_index(
    app: AppHandle,
    embedding_runtime: State<'_, ai::EmbeddingRuntimeState>,
    sealed_kernel: State<'_, SealedKernelState>,
) -> Result<u32, AppError> {
    let config = require_embedding_config(&app)?;

    sealed_kernel::clear_ai_embeddings(sealed_kernel.inner())?;
    let all_notes =
        sealed_kernel::prepare_ai_embedding_refresh_jobs("", true, sealed_kernel.inner())?
            .into_iter()
            .map(|job| (job.id, job.content))
            .collect::<Vec<_>>();
    let kernel_session = sealed_kernel::active_session_token(sealed_kernel.inner())?;
    let embedding_runtime = embedding_runtime.inner().clone();

    let results: Vec<_> = stream::iter(all_notes)
        .map(|(id, content)| {
            let config = config.clone();
            let embedding_runtime = embedding_runtime.clone();
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

/// 只为指定的相对路径列表刷新 kernel-backed Markdown 内容缓存并触发 embedding。
#[tauri::command]
pub fn index_changed_entries(
    vault_path: String,
    paths: Vec<String>,
    app: AppHandle,
    embedding_runtime: State<ai::EmbeddingRuntimeState>,
    sealed_kernel: State<SealedKernelState>,
) -> Result<u32, AppError> {
    sealed_kernel::ensure_vault_open(&vault_path, sealed_kernel.inner())?;
    let refresh_jobs =
        sealed_kernel::prepare_changed_ai_embedding_refresh_jobs(&paths, sealed_kernel.inner())?;

    spawn_prepared_embedding_jobs(
        refresh_jobs,
        &app,
        embedding_runtime.inner(),
        sealed_kernel.inner(),
    )
}
