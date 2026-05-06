use std::ffi::CStr;
use std::os::raw::{c_char, c_int};

use crate::{AppError, AppResult};

use super::ffi::*;

pub(super) fn take_bridge_string(ptr: *mut c_char) -> String {
    if ptr.is_null() {
        return String::new();
    }

    let value = unsafe { CStr::from_ptr(ptr).to_string_lossy().into_owned() };
    unsafe { sealed_kernel_bridge_free_string(ptr) };
    value
}

pub(super) fn bridge_error(operation: &str, code: c_int, raw_error: *mut c_char) -> AppError {
    let message = take_bridge_string(raw_error);
    let detail = if message.is_empty() {
        format!("{operation} failed with kernel status {code}.")
    } else {
        format!("{operation} failed with kernel status {code}: {message}")
    };
    AppError::Custom(detail)
}

pub(super) fn truth_diff_bridge_error(code: c_int, raw_error: *mut c_char) -> AppError {
    let token = take_bridge_string(raw_error);
    let message = match token.as_str() {
        "invalid_argument" => "Truth diff 内核参数无效".to_string(),
        "invalid_payload" => "Truth diff 内核返回结果无效".to_string(),
        "allocation_failed" => "Truth diff 内核结果分配失败".to_string(),
        "truth_diff_failed" | "" => "Truth diff 内核计算失败".to_string(),
        other => format!("Truth diff 内核计算失败 ({other}, status {code})"),
    };
    AppError::Custom(message)
}

pub(super) fn semantic_context_bridge_error(code: c_int, raw_error: *mut c_char) -> AppError {
    let token = take_bridge_string(raw_error);
    let message = match token.as_str() {
        "invalid_argument" => "语义上下文内核参数无效".to_string(),
        "allocation_failed" => "语义上下文内核结果分配失败".to_string(),
        "semantic_context_failed" | "" => "语义上下文内核计算失败".to_string(),
        other => format!("语义上下文内核计算失败 ({other}, status {code})"),
    };
    AppError::Custom(message)
}

pub(super) fn product_text_bridge_error(
    operation: &str,
    code: c_int,
    raw_error: *mut c_char,
) -> AppError {
    bridge_error(operation, code, raw_error)
}

pub(super) fn embedding_text_bridge_error(code: c_int, raw_error: *mut c_char) -> AppError {
    let token = take_bridge_string(raw_error);
    let message = match token.as_str() {
        "empty_text" => "文本内容为空，跳过向量化".to_string(),
        "invalid_argument" => "Embedding 输入内核参数无效".to_string(),
        "allocation_failed" => "Embedding 输入内核结果分配失败".to_string(),
        "normalize_embedding_text_failed" | "" => "Embedding 输入内核归一化失败".to_string(),
        other => format!("Embedding 输入内核归一化失败 ({other}, status {code})"),
    };
    AppError::Custom(message)
}

pub(super) fn spectroscopy_bridge_error(
    extension: &str,
    code: c_int,
    raw_error: *mut c_char,
) -> AppError {
    let token = take_bridge_string(raw_error);
    let message = match token.as_str() {
        "unsupported_extension" => format!("不支持的波谱文件扩展名: {extension}"),
        "csv_no_numeric_rows" => "CSV 中未找到有效的数值数据行".to_string(),
        "csv_too_few_columns" => "CSV 列数不足，至少需要 2 列".to_string(),
        "csv_no_valid_points" => "无法从 CSV 中提取有效数据点".to_string(),
        "jdx_no_points" => "JDX 文件中未找到可解析的数据点".to_string(),
        _ => format!("sealed_kernel_parse_spectroscopy_text failed with kernel status {code}."),
    };
    AppError::Custom(message)
}

pub(super) fn molecular_preview_bridge_error(
    extension: &str,
    code: c_int,
    raw_error: *mut c_char,
) -> AppError {
    let token = take_bridge_string(raw_error);
    let message = match token.as_str() {
        "unsupported_extension" => format!("不支持的分子文件扩展名: {extension}"),
        _ => format!("sealed_kernel_build_molecular_preview failed with kernel status {code}."),
    };
    AppError::Custom(message)
}

pub(super) fn retrosynthesis_bridge_error(code: c_int, raw_error: *mut c_char) -> AppError {
    let token = take_bridge_string(raw_error);
    let message = match token.as_str() {
        "invalid_argument" => "请输入目标分子 SMILES".to_string(),
        "empty_tree" | "invalid_payload" | "retro_failed" => "未生成可用逆合成路径".to_string(),
        _ => {
            format!("sealed_kernel_generate_mock_retrosynthesis failed with kernel status {code}.")
        }
    };
    AppError::Custom(message)
}

pub(super) fn kinetics_bridge_error(code: c_int, raw_error: *mut c_char) -> AppError {
    let token = take_bridge_string(raw_error);
    let message = match token.as_str() {
        "invalid_argument" => "聚合动力学参数无效".to_string(),
        "empty_result" | "invalid_payload" | "kinetics_failed" => {
            "聚合动力学内核计算失败".to_string()
        }
        _ => format!("sealed bridge kinetics failed with kernel status {code}."),
    };
    AppError::Custom(message)
}

pub(super) fn stoichiometry_bridge_error(code: c_int, raw_error: *mut c_char) -> AppError {
    let token = take_bridge_string(raw_error);
    let message = match token.as_str() {
        "invalid_argument" => "化学计量参数无效".to_string(),
        "allocation_failed" => "化学计量内核结果分配失败".to_string(),
        "stoichiometry_failed" | "" => "化学计量内核计算失败".to_string(),
        _ => format!("sealed bridge stoichiometry failed with kernel status {code}."),
    };
    AppError::Custom(message)
}

pub(super) fn ink_smoothing_bridge_error(code: c_int, raw_error: *mut c_char) -> AppError {
    let token = take_bridge_string(raw_error);
    let message = match token.as_str() {
        "invalid_argument" => "笔迹平滑参数无效".to_string(),
        "invalid_payload" | "ink_smoothing_failed" => "笔迹平滑内核计算失败".to_string(),
        _ => format!("sealed bridge ink smoothing failed with kernel status {code}."),
    };
    AppError::Custom(message)
}

pub(super) fn symmetry_bridge_error(
    format: &str,
    max_atoms: usize,
    code: c_int,
    raw_error: *mut c_char,
) -> AppError {
    let token = take_bridge_string(raw_error);
    let message = match token.as_str() {
        "parse_unsupported_format" => format!("不支持的分子文件格式: {format}"),
        "parse_xyz_empty" => "XYZ 文件为空".to_string(),
        "parse_xyz_incomplete" => "XYZ 文件格式不完整".to_string(),
        "parse_xyz_coordinate" => "XYZ 坐标解析失败".to_string(),
        "parse_pdb_coordinate" => "PDB 坐标解析失败".to_string(),
        "parse_cif_missing_cell" => {
            "CIF 使用分数坐标，但缺少完整晶胞参数 (_cell_length_*/_cell_angle_*)".to_string()
        }
        "parse_cif_invalid_cell" => "CIF 晶胞参数非法：无法构造有效的晶胞基矢".to_string(),
        "no_atoms" => "未找到任何原子坐标".to_string(),
        token if token.starts_with("too_many_atoms:") => {
            let atom_count = token
                .split_once(':')
                .and_then(|(_, value)| value.parse::<usize>().ok())
                .unwrap_or(0);
            format!("原子数 ({atom_count}) 超过对称性分析上限 ({max_atoms})，请使用较小的分子")
        }
        _ => format!("sealed_kernel_calculate_symmetry failed with kernel status {code}."),
    };
    AppError::Custom(message)
}

pub(super) fn atom_limit_bridge_error(
    surface: &str,
    code: c_int,
    raw_error: *mut c_char,
) -> AppError {
    let token = take_bridge_string(raw_error);
    let detail = if token.is_empty() {
        "unknown".to_string()
    } else {
        token
    };
    AppError::Custom(format!(
        "sealed kernel {surface} atom limit query failed with {detail} ({code})."
    ))
}

fn crystal_parse_error_message(token: &str) -> Option<String> {
    match token {
        "parse_missing_cell" => {
            Some("CIF 文件缺少完整的晶胞参数 (_cell_length_*/_cell_angle_*)".to_string())
        }
        "parse_missing_atoms" => {
            Some("CIF 文件中未找到分数坐标原子 (_atom_site_fract_*)".to_string())
        }
        _ => None,
    }
}

pub(super) fn crystal_lattice_bridge_error<F>(
    code: c_int,
    raw_error: *mut c_char,
    atom_limit: F,
) -> AppError
where
    F: FnOnce() -> AppResult<usize>,
{
    let token = take_bridge_string(raw_error);
    let message = crystal_parse_error_message(&token).unwrap_or_else(|| match token.as_str() {
        "supercell_gamma_too_small" => "晶胞参数非法：gamma 角过小".to_string(),
        "supercell_invalid_basis" => "晶胞参数非法：无法构造有效基矢".to_string(),
        token if token.starts_with("supercell_too_many_atoms:") => {
            let estimated_count = token
                .split_once(':')
                .and_then(|(_, value)| value.parse::<u64>().ok())
                .unwrap_or(0);
            match atom_limit() {
                Ok(limit) => {
                    format!("超晶胞原子数 ({estimated_count}) 超过上限 ({limit})，请减小扩展维度")
                }
                Err(_) => {
                    format!("超晶胞原子数 ({estimated_count}) 超过 kernel 上限，请减小扩展维度")
                }
            }
        }
        _ => format!("sealed_kernel_build_lattice_from_cif failed with kernel status {code}."),
    });
    AppError::Custom(message)
}

pub(super) fn crystal_miller_bridge_error(code: c_int, raw_error: *mut c_char) -> AppError {
    let token = take_bridge_string(raw_error);
    let message = crystal_parse_error_message(&token).unwrap_or_else(|| match token.as_str() {
        "miller_zero_index" => "密勒指数 (h, k, l) 不能全为零".to_string(),
        "miller_gamma_too_small" => "晶胞参数非法：gamma 角过小".to_string(),
        "miller_invalid_basis" => "晶胞参数非法：无法构造有效基矢".to_string(),
        "miller_zero_volume" => "晶胞体积为零，无法计算密勒面".to_string(),
        "miller_zero_normal" => "法向量长度为零".to_string(),
        _ => {
            format!(
                "sealed_kernel_calculate_miller_plane_from_cif failed with kernel status {code}."
            )
        }
    });
    AppError::Custom(message)
}
