//! AI 模块
//!
//! 负责调用云端 Embedding / Chat API。
//! 所有函数通过 AiConfig 参数接收配置，不再依赖环境变量。

mod chat;
mod embedding;
mod similarity;
pub mod vector_cache;

pub use chat::{build_rag_context_from_note_paths, ponder_node, stream_chat_with_context};
pub use embedding::{fetch_embedding_cached, EmbeddingRuntimeState};
pub use vector_cache::VectorCacheState;

/// AI 配置，由前端 Store 传入
#[derive(Debug, Clone)]
pub struct AiConfig {
    pub api_key: String,
    pub base_url: String,
    pub embedding_api_key: String,
    pub embedding_base_url: String,
    pub embedding_model: String,
    pub chat_model: String,
}
