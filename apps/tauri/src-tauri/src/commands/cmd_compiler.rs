use tauri::State;

use crate::compiler::{
    compile_markdown_to_pdf, CompilePayload, CompilerEnvironmentStatus, CompilerState,
};
use crate::AppError;

#[tauri::command]
pub async fn compile_to_pdf(
    payload: CompilePayload,
    compiler: State<'_, CompilerState>,
) -> Result<Vec<u8>, AppError> {
    compile_markdown_to_pdf(payload, compiler.inner())
        .await
        .map_err(Into::into)
}

#[tauri::command]
pub fn get_compiler_status(
    compiler: State<'_, CompilerState>,
) -> Result<CompilerEnvironmentStatus, AppError> {
    Ok(compiler.status())
}
