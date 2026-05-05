//! Chat 流式对话与 Ponder 节点生成

use futures_util::StreamExt;
use reqwest::Client;
use serde::{Deserialize, Serialize};
use std::time::Duration;

use crate::sealed_kernel;

use super::AiConfig;

#[derive(Serialize)]
struct ChatRequest {
    model: String,
    messages: Vec<ChatMessage>,
    stream: bool,
}

#[derive(Serialize, Clone)]
struct ChatMessage {
    role: String,
    content: String,
}

#[derive(Deserialize)]
struct ChatChunkResponse {
    choices: Vec<ChatChunkChoice>,
}

#[derive(Deserialize)]
struct ChatChunkChoice {
    delta: ChatChunkDelta,
}

#[derive(Deserialize)]
struct ChatChunkDelta {
    content: Option<String>,
}

#[derive(Serialize)]
struct ChatCompletionRequest {
    model: String,
    messages: Vec<ChatMessage>,
    stream: bool,
    temperature: f32,
}

#[derive(Deserialize)]
struct ChatCompletionResponse {
    choices: Vec<ChatCompletionChoice>,
}

#[derive(Deserialize)]
struct ChatCompletionChoice {
    message: ChatCompletionMessage,
}

#[derive(Deserialize)]
struct ChatCompletionMessage {
    content: String,
}

pub async fn stream_chat_with_context<F>(
    question: &str,
    context: &str,
    config: &AiConfig,
    mut on_chunk: F,
) -> Result<(), String>
where
    F: FnMut(String) -> Result<(), String>,
{
    if config.api_key.is_empty() {
        return Err("未配置 AI API Key，请在设置中填写".to_string());
    }

    let system_content =
        sealed_kernel::build_ai_rag_system_content(context).map_err(|err| err.to_string())?;

    let messages = vec![
        ChatMessage {
            role: "system".to_string(),
            content: system_content,
        },
        ChatMessage {
            role: "user".to_string(),
            content: question.to_string(),
        },
    ];

    let request_body = ChatRequest {
        model: config.chat_model.clone(),
        messages,
        stream: true,
    };

    let timeout_secs = sealed_kernel::ai_chat_timeout_secs().map_err(|err| err.to_string())?;
    let client = Client::builder()
        .timeout(Duration::from_secs(timeout_secs))
        .build()
        .map_err(|e| format!("创建 HTTP 客户端失败: {}", e))?;

    let url = format!("{}/chat/completions", config.base_url);

    let response = client
        .post(&url)
        .header("Authorization", format!("Bearer {}", config.api_key))
        .header("Content-Type", "application/json")
        .json(&request_body)
        .send()
        .await
        .map_err(|e| format!("Chat API 请求失败: {}", e))?;

    let status = response.status();
    if !status.is_success() {
        let body = response.text().await.unwrap_or_default();
        return Err(format!("Chat API 返回错误 (HTTP {}): {}", status, body));
    }

    let mut stream = response.bytes_stream();
    let mut buffer = String::new();

    while let Some(chunk_result) = stream.next().await {
        let chunk = chunk_result.map_err(|e| format!("读取流数据失败: {}", e))?;
        let text = String::from_utf8_lossy(&chunk);
        buffer.push_str(&text);

        while let Some(pos) = buffer.find("\n\n") {
            let line = buffer[..pos].to_string();
            buffer = buffer[pos + 2..].to_string();

            for sub_line in line.split('\n') {
                let sub_line = sub_line.trim();
                if sub_line.is_empty() {
                    continue;
                }

                if let Some(data) = sub_line.strip_prefix("data: ") {
                    let data = data.trim();
                    if data == "[DONE]" {
                        return Ok(());
                    }

                    if let Ok(parsed) = serde_json::from_str::<ChatChunkResponse>(data) {
                        if let Some(choice) = parsed.choices.first() {
                            if let Some(content) = &choice.delta.content {
                                if !content.is_empty() {
                                    on_chunk(content.clone())?;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Ok(())
}

pub async fn ponder_node(topic: &str, context: &str, config: &AiConfig) -> Result<String, String> {
    if config.api_key.is_empty() {
        return Err("未配置 AI API Key，请在设置中填写".to_string());
    }

    let system_prompt = sealed_kernel::ai_ponder_system_prompt().map_err(|err| err.to_string())?;
    let user_prompt = sealed_kernel::build_ai_ponder_user_prompt(topic, context)
        .map_err(|err| err.to_string())?;
    let temperature = sealed_kernel::ai_ponder_temperature().map_err(|err| err.to_string())?;

    let request_body = ChatCompletionRequest {
        model: config.chat_model.clone(),
        messages: vec![
            ChatMessage {
                role: "system".to_string(),
                content: system_prompt,
            },
            ChatMessage {
                role: "user".to_string(),
                content: user_prompt,
            },
        ],
        stream: false,
        temperature,
    };

    let timeout_secs = sealed_kernel::ai_ponder_timeout_secs().map_err(|err| err.to_string())?;
    let client = Client::builder()
        .timeout(Duration::from_secs(timeout_secs))
        .build()
        .map_err(|e| format!("创建 HTTP 客户端失败: {}", e))?;

    let url = format!("{}/chat/completions", config.base_url);

    let response = client
        .post(&url)
        .header("Authorization", format!("Bearer {}", config.api_key))
        .header("Content-Type", "application/json")
        .json(&request_body)
        .send()
        .await
        .map_err(|e| format!("Ponder API 请求失败: {}", e))?;

    let status = response.status();
    if !status.is_success() {
        let body = response.text().await.unwrap_or_default();
        return Err(format!("Ponder API 返回错误 (HTTP {}): {}", status, body));
    }

    let result: ChatCompletionResponse = response
        .json()
        .await
        .map_err(|e| format!("解析 Ponder API 响应失败: {}", e))?;

    result
        .choices
        .into_iter()
        .next()
        .map(|c| c.message.content.trim().to_string())
        .filter(|s| !s.is_empty())
        .ok_or_else(|| "Ponder API 返回空内容".to_string())
}
