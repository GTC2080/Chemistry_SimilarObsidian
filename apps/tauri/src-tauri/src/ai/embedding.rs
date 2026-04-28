//! Embedding 请求、缓存与并发控制

use reqwest::Client;
use serde::{Deserialize, Serialize};
use std::collections::{HashMap, VecDeque};
use std::sync::{Arc, Mutex};
use std::time::Duration;
use tokio::sync::Semaphore;

use crate::sealed_kernel;

use super::AiConfig;

#[derive(Serialize)]
struct EmbeddingRequest {
    model: String,
    input: String,
}

#[derive(Deserialize)]
struct EmbeddingData {
    embedding: Vec<f32>,
}

#[derive(Deserialize)]
struct EmbeddingResponse {
    data: Vec<EmbeddingData>,
}

#[derive(Default)]
struct EmbeddingCache {
    order: VecDeque<String>,
    entries: HashMap<String, Vec<f32>>,
}

#[derive(Clone)]
pub struct EmbeddingRuntimeState {
    semaphore: Arc<Semaphore>,
    cache: Arc<Mutex<EmbeddingCache>>,
    /// Shared HTTP client — reused across all embedding requests.
    client: Client,
    /// Per-note version counter for deduplication.
    /// Before writing an embedding result back to DB, check that the version
    /// hasn't been bumped by a newer save.
    pub note_versions: Arc<Mutex<HashMap<String, u64>>>,
}

impl Default for EmbeddingRuntimeState {
    fn default() -> Self {
        let concurrency_limit = sealed_kernel::ai_embedding_concurrency_limit()
            .expect("kernel AI embedding concurrency limit should be available");
        let request_timeout_secs = sealed_kernel::ai_embedding_request_timeout_secs()
            .expect("kernel AI embedding request timeout should be available");
        Self {
            semaphore: Arc::new(Semaphore::new(concurrency_limit)),
            cache: Arc::new(Mutex::new(EmbeddingCache::default())),
            client: Client::builder()
                .timeout(Duration::from_secs(request_timeout_secs))
                .build()
                .expect("failed to create reqwest::Client"),
            note_versions: Arc::new(Mutex::new(HashMap::new())),
        }
    }
}

impl EmbeddingRuntimeState {
    /// Bump the version for a note and return the new version number.
    /// Callers should store this and check it before writing the embedding result.
    pub fn bump_version(&self, note_id: &str) -> u64 {
        let mut versions = self.note_versions.lock().unwrap_or_else(|e| e.into_inner());
        let v = versions.entry(note_id.to_string()).or_insert(0);
        *v += 1;
        *v
    }

    /// Check if the given version is still current for this note.
    pub fn is_current_version(&self, note_id: &str, version: u64) -> bool {
        let versions = self.note_versions.lock().unwrap_or_else(|e| e.into_inner());
        versions.get(note_id).copied().unwrap_or(0) == version
    }
}

fn normalize_embedding_text(text: &str) -> Result<String, String> {
    sealed_kernel::normalize_ai_embedding_text(text).map_err(|err| err.to_string())
}

fn embedding_cache_key(text: &str, config: &AiConfig) -> String {
    use std::collections::hash_map::DefaultHasher;
    use std::hash::{Hash, Hasher};
    let base_url = if config.embedding_base_url.is_empty() {
        &config.base_url
    } else {
        &config.embedding_base_url
    };
    let mut h = DefaultHasher::new();
    base_url.hash(&mut h);
    config.embedding_model.hash(&mut h);
    text.hash(&mut h);
    format!("{:016x}", h.finish())
}

fn get_cached_embedding(
    runtime: &EmbeddingRuntimeState,
    key: &str,
) -> Result<Option<Vec<f32>>, String> {
    let cache = runtime
        .cache
        .lock()
        .map_err(|e| format!("获取 embedding 缓存锁失败: {}", e))?;
    Ok(cache.entries.get(key).cloned())
}

fn cache_embedding(
    runtime: &EmbeddingRuntimeState,
    key: String,
    embedding: Vec<f32>,
) -> Result<(), String> {
    let cache_limit = sealed_kernel::ai_embedding_cache_limit().map_err(|err| err.to_string())?;
    let mut cache = runtime
        .cache
        .lock()
        .map_err(|e| format!("获取 embedding 缓存锁失败: {}", e))?;

    if cache.entries.contains_key(&key) {
        cache.order.retain(|existing| existing != &key);
    }

    cache.order.push_back(key.clone());
    cache.entries.insert(key, embedding);

    while cache.order.len() > cache_limit {
        if let Some(oldest) = cache.order.pop_front() {
            cache.entries.remove(&oldest);
        }
    }

    Ok(())
}

pub async fn fetch_embedding(
    text: &str,
    config: &AiConfig,
    client: &Client,
) -> Result<Vec<f32>, String> {
    let effective_key = if config.embedding_api_key.is_empty() {
        &config.api_key
    } else {
        &config.embedding_api_key
    };
    if effective_key.is_empty() {
        return Err("未配置 AI API Key，请在设置中填写".to_string());
    }

    let truncated = normalize_embedding_text(text)?;

    let url = format!(
        "{}/embeddings",
        if config.embedding_base_url.is_empty() {
            &config.base_url
        } else {
            &config.embedding_base_url
        }
    );

    let request_body = EmbeddingRequest {
        model: config.embedding_model.clone(),
        input: truncated,
    };

    let response = client
        .post(&url)
        .header("Authorization", format!("Bearer {}", effective_key))
        .header("Content-Type", "application/json")
        .json(&request_body)
        .send()
        .await
        .map_err(|e| format!("Embedding API 请求失败: {}", e))?;

    let status = response.status();
    if !status.is_success() {
        let body = response.text().await.unwrap_or_default();
        return Err(format!(
            "Embedding API 返回错误 (HTTP {}): {}",
            status, body
        ));
    }

    let result: EmbeddingResponse = response
        .json()
        .await
        .map_err(|e| format!("解析 Embedding API 响应失败: {}", e))?;

    result
        .data
        .into_iter()
        .next()
        .map(|d| d.embedding)
        .ok_or_else(|| "Embedding API 返回了空的 data 数组".to_string())
}

pub async fn fetch_embedding_cached(
    text: &str,
    config: &AiConfig,
    runtime: &EmbeddingRuntimeState,
) -> Result<Vec<f32>, String> {
    let normalized = normalize_embedding_text(text)?;
    let cache_key = embedding_cache_key(&normalized, config);

    if let Some(cached) = get_cached_embedding(runtime, &cache_key)? {
        return Ok(cached);
    }

    let _permit = runtime
        .semaphore
        .acquire()
        .await
        .map_err(|e| format!("获取 embedding 并发许可失败: {}", e))?;

    if let Some(cached) = get_cached_embedding(runtime, &cache_key)? {
        return Ok(cached);
    }

    let embedding = fetch_embedding(&normalized, config, &runtime.client).await?;
    cache_embedding(runtime, cache_key, embedding.clone())?;
    Ok(embedding)
}
