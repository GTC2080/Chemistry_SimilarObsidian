use tauri::AppHandle;
use tauri_plugin_store::StoreExt;

use crate::ai::AiConfig;

pub fn read_ai_config(app: &AppHandle) -> Result<AiConfig, String> {
    let store = app
        .store("settings.json")
        .map_err(|e| format!("打开 Store 失败: {}", e))?;

    let setting = |key: &str| {
        store
            .get(key)
            .and_then(|value| value.as_str().map(str::to_string))
    };

    let api_key = setting("aiApiKey").unwrap_or_default();
    let base_url = setting("aiBaseUrl").unwrap_or_else(|| "https://api.openai.com/v1".to_string());
    let embedding_base_url = setting("embeddingBaseUrl").unwrap_or_default();
    let embedding_api_key = setting("embeddingApiKey").unwrap_or_default();
    let embedding_model =
        setting("embeddingModel").unwrap_or_else(|| "text-embedding-3-small".to_string());
    let chat_model = setting("chatModel").unwrap_or_else(|| "gpt-4o-mini".to_string());

    Ok(AiConfig {
        api_key,
        base_url,
        embedding_api_key,
        embedding_base_url,
        embedding_model,
        chat_model,
    })
}
