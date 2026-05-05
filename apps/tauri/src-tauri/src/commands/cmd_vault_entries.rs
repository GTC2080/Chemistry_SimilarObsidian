use tauri::State;

use crate::sealed_kernel::{self, SealedKernelState};
use crate::AppError;

#[tauri::command]
pub async fn delete_entry(
    vault_path: String,
    target_path: String,
    sealed_kernel: State<'_, SealedKernelState>,
) -> Result<(), AppError> {
    sealed_kernel::delete_entry_by_path(&vault_path, &target_path, sealed_kernel.inner())
}

#[tauri::command]
pub async fn move_entry(
    vault_path: String,
    source_path: String,
    dest_folder: String,
    sealed_kernel: State<'_, SealedKernelState>,
) -> Result<(), AppError> {
    sealed_kernel::move_entry_by_path(
        &vault_path,
        &source_path,
        &dest_folder,
        sealed_kernel.inner(),
    )
}

#[tauri::command]
pub async fn create_folder(
    vault_path: String,
    folder_path: String,
    sealed_kernel: State<'_, SealedKernelState>,
) -> Result<(), AppError> {
    sealed_kernel::create_folder_by_path(&vault_path, &folder_path, sealed_kernel.inner())
}

#[tauri::command]
pub async fn rename_entry(
    vault_path: String,
    source_path: String,
    new_name: String,
    sealed_kernel: State<'_, SealedKernelState>,
) -> Result<(), AppError> {
    sealed_kernel::rename_entry_by_path(&vault_path, &source_path, &new_name, sealed_kernel.inner())
}
