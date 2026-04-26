use crate::AppError;
use crate::{sealed_kernel, symmetry};

/// 计算分子对称性（点群、镜面、旋转轴）
///
/// 前端调用方式：invoke('calculate_symmetry', { data, format })
/// - data: 分子文件的纯文本内容 (PDB/XYZ/CIF)
/// - format: 文件扩展名 ("pdb" | "xyz" | "cif")
///
/// 返回 SymmetryData JSON，包含预计算的渲染几何数据，前端零计算直接绘图。
#[tauri::command]
pub fn calculate_symmetry(
    data: String,
    format: String,
) -> Result<symmetry::SymmetryData, AppError> {
    sealed_kernel::calculate_symmetry_from_text(&data, &format)
}
