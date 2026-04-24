use crate::chem_api::{self, CompoundInfo, RetroTreeData};
use crate::kinetics::{self, KineticsParams, KineticsResult};
use crate::AppError;

#[tauri::command]
pub async fn fetch_compound_info(query: String) -> Result<CompoundInfo, AppError> {
    chem_api::fetch_compound_info(query)
        .await
        .map_err(Into::into)
}

#[tauri::command]
pub async fn retrosynthesize_target(
    target_smiles: String,
    depth: u8,
) -> Result<RetroTreeData, AppError> {
    chem_api::retrosynthesize_target(target_smiles, depth)
        .await
        .map_err(Into::into)
}

#[tauri::command]
pub async fn simulate_polymerization(params: KineticsParams) -> Result<KineticsResult, AppError> {
    tauri::async_runtime::spawn_blocking(move || kinetics::simulate_polymerization(params))
        .await
        .map_err(|_| AppError::Custom("模拟任务执行失败".to_string()))?
        .map_err(Into::into)
}
