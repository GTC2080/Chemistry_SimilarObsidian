use std::env;
use std::path::{Path, PathBuf};
use std::process::Command as StdCommand;
use std::process::Stdio;
use std::time::{SystemTime, UNIX_EPOCH};

use serde::{Deserialize, Serialize};
use tokio::fs;
use tokio::process::Command as TokioCommand;

#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct CompilerEnvironmentStatus {
    pub ready: bool,
    pub pandoc_available: bool,
    pub latex_engine_available: bool,
    pub message: String,
}

#[derive(Debug, Clone)]
pub struct CompilerState {
    status: CompilerEnvironmentStatus,
}

#[derive(Debug, Clone, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct CompilePayload {
    pub markdown: String,
    #[serde(default)]
    pub image_paths: Vec<String>,
    #[serde(default = "default_template")]
    pub template: String,
    #[serde(default)]
    pub csl_path: Option<String>,
    #[serde(default)]
    pub bibliography_path: Option<String>,
}

fn default_template() -> String {
    "standard-thesis".to_string()
}

impl CompilerState {
    pub fn detect() -> Self {
        let pandoc = detect_tool("pandoc", &["-v"]);
        let latex = detect_tool("xelatex", &["--version"]);
        let pandoc_available = pandoc.is_ok();
        let latex_engine_available = latex.is_ok();

        let status = if pandoc_available && latex_engine_available {
            CompilerEnvironmentStatus {
                ready: true,
                pandoc_available,
                latex_engine_available,
                message: "排版编译环境已就绪（Pandoc + XeLaTeX）".to_string(),
            }
        } else {
            let mut hints: Vec<String> = Vec::new();
            if let Err(reason) = pandoc {
                hints.push(format!(
                    "未检测到 Pandoc：{}。请安装 Pandoc（https://pandoc.org/installing.html）。",
                    reason
                ));
            }
            if let Err(reason) = latex {
                hints.push(format!(
                    "未检测到 XeLaTeX：{}。请安装 TeX 发行版（TeX Live / MacTeX / MiKTeX），并确保 `xelatex` 可在终端直接运行。",
                    reason
                ));
            }
            CompilerEnvironmentStatus {
                ready: false,
                pandoc_available,
                latex_engine_available,
                message: hints.join("\n"),
            }
        };

        Self { status }
    }

    pub fn status(&self) -> CompilerEnvironmentStatus {
        self.status.clone()
    }

    pub fn ensure_ready(&self) -> Result<(), String> {
        if self.status.ready {
            return Ok(());
        }
        Err(self.status.message.clone())
    }
}

fn detect_tool(binary: &str, args: &[&str]) -> Result<(), String> {
    let output = StdCommand::new(binary)
        .args(args)
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .output()
        .map_err(|e| e.to_string())?;
    if output.status.success() {
        Ok(())
    } else {
        Err(format!("{} 返回非零退出码", binary))
    }
}

fn build_temp_workspace() -> PathBuf {
    let stamp = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_millis();
    env::temp_dir().join(format!("nexus-paper-{}-{}", std::process::id(), stamp))
}

fn append_template_args(command: &mut TokioCommand, template: &str) {
    match template.trim().to_ascii_lowercase().as_str() {
        "acs" => {
            command.args(["-V", "papersize=letter"]);
            command.args(["-V", "fontsize=10pt"]);
            command.args(["-V", "geometry:margin=1in"]);
        }
        "nature" => {
            command.args(["-V", "papersize=a4"]);
            command.args(["-V", "fontsize=10pt"]);
            command.args(["-V", "geometry:margin=1in"]);
        }
        "standard-thesis" => {
            command.args(["-V", "documentclass=report"]);
            command.args(["-V", "fontsize=12pt"]);
            command.args(["-V", "geometry:margin=1in"]);
        }
        _ => {
            command.args(["-V", "documentclass=article"]);
            command.args(["-V", "fontsize=11pt"]);
        }
    }
}

fn resource_path_separator() -> &'static str {
    #[cfg(target_os = "windows")]
    {
        ";"
    }
    #[cfg(not(target_os = "windows"))]
    {
        ":"
    }
}

fn build_resource_path(image_paths: &[String], workspace: &Path) -> Option<String> {
    let mut roots: Vec<String> = Vec::new();
    roots.push(workspace.to_string_lossy().to_string());

    for path in image_paths {
        let parent = Path::new(path).parent();
        if let Some(dir) = parent {
            let normalized = dir.to_string_lossy().to_string();
            if !normalized.trim().is_empty()
                && !roots.iter().any(|existing| existing == &normalized)
            {
                roots.push(normalized);
            }
        }
    }

    if roots.is_empty() {
        None
    } else {
        Some(roots.join(resource_path_separator()))
    }
}

fn trim_log(log: &str, limit: usize) -> String {
    if log.chars().count() <= limit {
        return log.to_string();
    }
    let mut trimmed = String::new();
    for (idx, ch) in log.chars().enumerate() {
        if idx >= limit {
            break;
        }
        trimmed.push(ch);
    }
    trimmed.push_str("\n...（日志已截断）");
    trimmed
}

fn extract_latex_error(log: &str) -> String {
    let mut highlights: Vec<String> = Vec::new();
    for line in log.lines() {
        let lower = line.to_ascii_lowercase();
        if lower.contains("! ")
            || lower.contains("error")
            || lower.contains("undefined control sequence")
            || lower.contains("missing")
            || lower.contains("emergency stop")
            || lower.contains("fatal")
        {
            highlights.push(line.trim().to_string());
        }
        if highlights.len() >= 12 {
            break;
        }
    }

    if highlights.is_empty() {
        "未提取到明确 LaTeX 关键报错，请查看完整编译日志。".to_string()
    } else {
        highlights.join("\n")
    }
}

fn command_spawn_error(binary: &str, err: &str) -> String {
    format!(
        "无法启动 `{}`：{}。\n请确认已正确安装对应工具并可在终端直接运行。",
        binary, err
    )
}

pub async fn compile_markdown_to_pdf(
    payload: CompilePayload,
    state: &CompilerState,
) -> Result<Vec<u8>, String> {
    state.ensure_ready()?;

    let workspace = build_temp_workspace();
    fs::create_dir_all(&workspace)
        .await
        .map_err(|e| format!("创建临时编译目录失败: {}", e))?;

    let result = async {
        let markdown_path = workspace.join("build.md");
        let output_path = workspace.join("output.pdf");
        let header_path = workspace.join("header.tex");

        fs::write(&markdown_path, payload.markdown.as_bytes())
            .await
            .map_err(|e| format!("写入临时 Markdown 失败: {}", e))?;
        fs::write(
            &header_path,
            "\\usepackage[version=4]{mhchem}\n\\usepackage{booktabs}\n\\usepackage{longtable}\n",
        )
        .await
        .map_err(|e| format!("写入 LaTeX 头文件失败: {}", e))?;

        let mut command = TokioCommand::new("pandoc");
        command
            .arg(&markdown_path)
            .arg("-o")
            .arg(&output_path)
            .arg("--pdf-engine=xelatex")
            .arg("-V")
            .arg("colorlinks=true")
            .arg("--include-in-header")
            .arg(&header_path)
            .stdout(Stdio::piped())
            .stderr(Stdio::piped())
            .current_dir(&workspace);

        append_template_args(&mut command, &payload.template);

        if let Some(csl_path) = payload.csl_path.as_ref().filter(|p| !p.trim().is_empty()) {
            command.arg("--csl").arg(csl_path);
        }

        if let Some(bib_path) = payload
            .bibliography_path
            .as_ref()
            .filter(|p| !p.trim().is_empty())
        {
            command.arg("--bibliography").arg(bib_path);
        }

        if let Some(resource_path) = build_resource_path(&payload.image_paths, &workspace) {
            command.arg("--resource-path").arg(resource_path);
        }

        #[cfg(target_os = "windows")]
        {
            const CREATE_NO_WINDOW: u32 = 0x08000000;
            command.creation_flags(CREATE_NO_WINDOW);
        }

        let output = command
            .output()
            .await
            .map_err(|e| command_spawn_error("pandoc", &e.to_string()))?;

        if !output.status.success() {
            let stdout = String::from_utf8_lossy(&output.stdout).to_string();
            let stderr = String::from_utf8_lossy(&output.stderr).to_string();
            let merged = if !stderr.trim().is_empty() {
                stderr
            } else {
                stdout
            };
            let extracted = extract_latex_error(&merged);
            let code = output
                .status
                .code()
                .map(|v| v.to_string())
                .unwrap_or_else(|| "unknown".to_string());
            return Err(format!(
                "PDF 编译失败（exit code: {}）。\n{}\n\n--- 编译日志 ---\n{}",
                code,
                extracted,
                trim_log(&merged, 7000)
            ));
        }

        fs::read(&output_path)
            .await
            .map_err(|e| format!("读取输出 PDF 失败: {}", e))
    }
    .await;

    let _ = fs::remove_dir_all(&workspace).await;
    result
}
