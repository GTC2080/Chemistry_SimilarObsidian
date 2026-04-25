use std::collections::HashSet;

use tauri::AppHandle;
use tauri_plugin_store::StoreExt;

use crate::ai::AiConfig;

/// 支持的文件扩展名白名单
pub const SUPPORTED_EXTENSIONS: &[&str] = &[
    "md", "txt", "json", "py", "rs", "js", "ts", "jsx", "tsx", "css", "html", "toml", "yaml",
    "yml", "xml", "sh", "bat", "c", "cpp", "h", "java", "go", "png", "jpg", "jpeg", "gif", "svg",
    "webp", "bmp", "ico", "pdf", "mol", "chemdraw", "paper", "csv", "jdx", "pdb", "xyz", "cif",
];

pub fn read_ai_config(app: &AppHandle) -> Result<AiConfig, String> {
    let store = app
        .store("settings.json")
        .map_err(|e| format!("打开 Store 失败: {}", e))?;

    let api_key = store
        .get("aiApiKey")
        .and_then(|v| v.as_str().map(|s| s.to_string()))
        .unwrap_or_default();

    let base_url = store
        .get("aiBaseUrl")
        .and_then(|v| v.as_str().map(|s| s.to_string()))
        .unwrap_or_else(|| "https://api.openai.com/v1".to_string());

    let embedding_base_url = store
        .get("embeddingBaseUrl")
        .and_then(|v| v.as_str().map(|s| s.to_string()))
        .unwrap_or_default();

    let embedding_api_key = store
        .get("embeddingApiKey")
        .and_then(|v| v.as_str().map(|s| s.to_string()))
        .unwrap_or_default();

    let embedding_model = store
        .get("embeddingModel")
        .and_then(|v| v.as_str().map(|s| s.to_string()))
        .unwrap_or_else(|| "text-embedding-3-small".to_string());

    let chat_model = store
        .get("chatModel")
        .and_then(|v| v.as_str().map(|s| s.to_string()))
        .unwrap_or_else(|| "gpt-4o-mini".to_string());

    Ok(AiConfig {
        api_key,
        base_url,
        embedding_api_key,
        embedding_base_url,
        embedding_model,
        chat_model,
    })
}

pub fn parse_ignored_folders(ignored_folders: Option<String>) -> HashSet<String> {
    ignored_folders
        .unwrap_or_default()
        .split(',')
        .map(|s| s.trim().trim_matches('/').trim_matches('\\').to_string())
        .filter(|s| !s.is_empty())
        .collect()
}

pub fn is_supported_extension(ext: &str) -> bool {
    SUPPORTED_EXTENSIONS
        .iter()
        .any(|e| e.eq_ignore_ascii_case(ext))
}
