use std::fs;
use std::path::Path;

use tauri::AppHandle;
use tauri_plugin_store::StoreExt;

use crate::ai::AiConfig;

/// 支持的文件扩展名白名单
pub const SUPPORTED_EXTENSIONS: &[&str] = &[
    "md", "txt", "json", "py", "rs", "js", "ts", "jsx", "tsx", "css", "html", "toml", "yaml",
    "yml", "xml", "sh", "bat", "c", "cpp", "h", "java", "go", "png", "jpg", "jpeg", "gif", "svg",
    "webp", "bmp", "ico", "pdf", "mol", "chemdraw", "paper", "csv", "jdx", "pdb", "xyz", "cif",
];

/// 可以读取文本内容的扩展名（非二进制）
pub const TEXT_EXTENSIONS: &[&str] = &[
    "md", "txt", "json", "py", "rs", "js", "ts", "jsx", "tsx", "css", "html", "toml", "yaml",
    "yml", "xml", "sh", "bat", "c", "cpp", "h", "java", "go", "paper", "csv", "jdx", "pdb", "xyz",
    "cif",
];

/// 允许进行 AI 向量化的扩展名
pub const EMBEDDABLE_EXTENSIONS: &[&str] = &["md", "txt", "pdf"];

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

pub fn is_supported_extension(ext: &str) -> bool {
    SUPPORTED_EXTENSIONS
        .iter()
        .any(|e| e.eq_ignore_ascii_case(ext))
}

pub fn is_text_extension(ext: &str) -> bool {
    TEXT_EXTENSIONS.iter().any(|e| e.eq_ignore_ascii_case(ext))
}

pub fn is_embeddable_extension(ext: &str) -> bool {
    EMBEDDABLE_EXTENSIONS
        .iter()
        .any(|e| e.eq_ignore_ascii_case(ext))
}

pub fn is_mol_extension(ext: &str) -> bool {
    ext.eq_ignore_ascii_case("mol") || ext.eq_ignore_ascii_case("chemdraw")
}

pub fn is_pdf_extension(ext: &str) -> bool {
    ext.eq_ignore_ascii_case("pdf")
}

pub fn is_paper_extension(ext: &str) -> bool {
    ext.eq_ignore_ascii_case("paper")
}

pub fn is_spectroscopy_extension(ext: &str) -> bool {
    ext.eq_ignore_ascii_case("csv") || ext.eq_ignore_ascii_case("jdx")
}

pub fn is_molecular_extension(ext: &str) -> bool {
    ext.eq_ignore_ascii_case("pdb")
        || ext.eq_ignore_ascii_case("xyz")
        || ext.eq_ignore_ascii_case("cif")
}

/// 从 PDF 文件中提取纯文本内容（在大栈线程中运行，防止栈溢出崩溃）
pub fn extract_pdf_text(path: &Path) -> Result<String, String> {
    let bytes =
        fs::read(path).map_err(|e| format!("读取 PDF 文件失败 [{}]: {}", path.display(), e))?;

    let handle = std::thread::Builder::new()
        .name("pdf-extract".into())
        .stack_size(8 * 1024 * 1024)
        .spawn(move || std::panic::catch_unwind(|| pdf_extract::extract_text_from_mem(&bytes)))
        .map_err(|e| format!("创建 PDF 提取线程失败: {}", e))?;

    match handle.join() {
        Ok(Ok(Ok(text))) => Ok(text),
        Ok(Ok(Err(e))) => Err(format!("提取 PDF 文本失败 [{}]: {}", path.display(), e)),
        Ok(Err(_)) => Err(format!("提取 PDF 文本时发生 panic [{}]", path.display())),
        Err(_) => Err(format!("PDF 提取线程异常退出 [{}]", path.display())),
    }
}
