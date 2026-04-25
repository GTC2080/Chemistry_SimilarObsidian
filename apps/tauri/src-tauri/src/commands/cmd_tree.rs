use tauri::State;

use crate::models::FileTreeNode;
use crate::sealed_kernel::{self, SealedKernelState};
use crate::AppError;

#[tauri::command]
pub fn build_file_tree(
    vault_path: String,
    ignored_folders: Option<String>,
    sealed_kernel: State<SealedKernelState>,
) -> Result<Vec<FileTreeNode>, AppError> {
    if vault_path.trim().is_empty() {
        return Ok(Vec::new());
    }

    sealed_kernel::query_file_tree(
        &vault_path,
        sealed_kernel.inner(),
        4096,
        ignored_folders.as_deref().unwrap_or(""),
    )
}
