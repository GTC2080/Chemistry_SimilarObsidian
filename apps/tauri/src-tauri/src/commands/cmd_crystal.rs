use crate::crystal;
use crate::AppError;

/// 解析 CIF 文件并生成超晶胞
///
/// 前端调用方式：invoke('parse_and_build_lattice', { cifText, nx, ny, nz })
/// - cifText: CIF 文件纯文本
/// - nx, ny, nz: 超晶胞扩展维度（各方向重复次数）
///
/// 使用 spawn_blocking 在独立线程池执行，避免阻塞 Tauri 主通信线程
#[tauri::command]
pub async fn parse_and_build_lattice(
    cif_text: String,
    nx: u32,
    ny: u32,
    nz: u32,
) -> Result<crystal::LatticeData, AppError> {
    tauri::async_runtime::spawn_blocking(move || {
        crystal::parse_and_build_lattice(&cif_text, nx, ny, nz).map_err(AppError::from)
    })
    .await
    .map_err(|e| AppError::Custom(format!("线程执行错误: {}", e)))?
}

/// 计算密勒指数切割面
///
/// 前端调用方式：invoke('calculate_miller_plane', { cifText, h, k, l })
#[tauri::command]
pub async fn calculate_miller_plane(
    cif_text: String,
    h: i32,
    k: i32,
    l: i32,
) -> Result<crystal::MillerPlaneData, AppError> {
    tauri::async_runtime::spawn_blocking(move || {
        crystal::calculate_miller_plane(&cif_text, h, k, l).map_err(AppError::from)
    })
    .await
    .map_err(|e| AppError::Custom(format!("线程执行错误: {}", e)))?
}
