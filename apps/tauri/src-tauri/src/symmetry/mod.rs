//! 分子点群与空间对称性推演桥接层
//!
//! Tauri Rust 保留命令 DTO；解析、形状分析、主轴计算、候选生成、操作匹配、点群分类与渲染几何已由 C++ kernel 提供。
//! 前端零计算：几何数据（平面顶点、轴端点）在 host 命令返回前由 kernel 预计算完毕。

mod types;

use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::slice;

use types::{RotationAxis, SymmetryPlane, Vec3D};

pub use types::SymmetryData;

const MAX_ATOMS_FOR_SYMMETRY: usize = 500;
const KERNEL_OK: i32 = 0;
const KERNEL_SYMMETRY_POINT_GROUP_MAX: usize = 32;

const CALC_ERROR_PARSE: i32 = 1;
const CALC_ERROR_NO_ATOMS: i32 = 2;
const CALC_ERROR_TOO_MANY_ATOMS: i32 = 3;

#[repr(C)]
#[derive(Debug, Clone, Copy)]
struct KernelStatus {
    code: i32,
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
struct KernelSymmetryRenderAxis {
    vector: [f64; 3],
    center: [f64; 3],
    order: u8,
    start: [f64; 3],
    end: [f64; 3],
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
struct KernelSymmetryRenderPlane {
    normal: [f64; 3],
    center: [f64; 3],
    vertices: [[f64; 3]; 4],
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
struct KernelSymmetryCalculationResult {
    point_group: [c_char; KERNEL_SYMMETRY_POINT_GROUP_MAX],
    axes: *mut KernelSymmetryRenderAxis,
    axis_count: usize,
    planes: *mut KernelSymmetryRenderPlane,
    plane_count: usize,
    has_inversion: u8,
    atom_count: usize,
    error: i32,
    parse_error: i32,
}

impl Default for KernelSymmetryCalculationResult {
    fn default() -> Self {
        Self {
            point_group: [0; KERNEL_SYMMETRY_POINT_GROUP_MAX],
            axes: std::ptr::null_mut(),
            axis_count: 0,
            planes: std::ptr::null_mut(),
            plane_count: 0,
            has_inversion: 0,
            atom_count: 0,
            error: 0,
            parse_error: 0,
        }
    }
}

extern "C" {
    fn kernel_calculate_symmetry(
        raw: *const c_char,
        raw_size: usize,
        format: *const c_char,
        max_atoms: usize,
        out_result: *mut KernelSymmetryCalculationResult,
    ) -> KernelStatus;
    fn kernel_free_symmetry_calculation_result(result: *mut KernelSymmetryCalculationResult);
}

// ===== 公开接口 =====

pub fn calculate(raw_data: &str, format: &str) -> Result<SymmetryData, String> {
    let c_format = CString::new(format).map_err(|_| format!("不支持的分子文件格式: {}", format))?;
    let mut raw_result = KernelSymmetryCalculationResult::default();
    let status = unsafe {
        kernel_calculate_symmetry(
            raw_data.as_ptr() as *const c_char,
            raw_data.len(),
            c_format.as_ptr(),
            MAX_ATOMS_FOR_SYMMETRY,
            &mut raw_result,
        )
    };

    if status.code != KERNEL_OK {
        let message = calculation_error_message(&raw_result, format);
        unsafe { kernel_free_symmetry_calculation_result(&mut raw_result) };
        return Err(message);
    }

    let data = unsafe { symmetry_data_from_kernel(&raw_result) };
    unsafe { kernel_free_symmetry_calculation_result(&mut raw_result) };
    data
}

fn parse_error_message(error: i32, format: &str) -> String {
    match error {
        1 => format!("不支持的分子文件格式: {}", format),
        2 => "XYZ 文件为空".to_string(),
        3 => "XYZ 文件格式不完整".to_string(),
        4 => "XYZ 坐标解析失败".to_string(),
        5 => "PDB 坐标解析失败".to_string(),
        6 => "CIF 使用分数坐标，但缺少完整晶胞参数 (_cell_length_*/_cell_angle_*)".to_string(),
        7 => "CIF 晶胞参数非法：无法构造有效的晶胞基矢".to_string(),
        _ => "分子坐标解析失败".to_string(),
    }
}

fn calculation_error_message(result: &KernelSymmetryCalculationResult, format: &str) -> String {
    match result.error {
        CALC_ERROR_PARSE => parse_error_message(result.parse_error, format),
        CALC_ERROR_NO_ATOMS => "未找到任何原子坐标".to_string(),
        CALC_ERROR_TOO_MANY_ATOMS => format!(
            "原子数 ({}) 超过对称性分析上限 ({})，请使用较小的分子",
            result.atom_count, MAX_ATOMS_FOR_SYMMETRY
        ),
        _ => "kernel symmetry calculation failed".to_string(),
    }
}

fn vec3_from_kernel(value: [f64; 3]) -> Vec3D {
    Vec3D {
        x: value[0],
        y: value[1],
        z: value[2],
    }
}

fn axis_from_kernel(axis: &KernelSymmetryRenderAxis) -> RotationAxis {
    RotationAxis {
        vector: vec3_from_kernel(axis.vector),
        center: vec3_from_kernel(axis.center),
        order: axis.order,
        start: vec3_from_kernel(axis.start),
        end: vec3_from_kernel(axis.end),
    }
}

fn plane_from_kernel(plane: &KernelSymmetryRenderPlane) -> SymmetryPlane {
    SymmetryPlane {
        normal: vec3_from_kernel(plane.normal),
        center: vec3_from_kernel(plane.center),
        vertices: [
            vec3_from_kernel(plane.vertices[0]),
            vec3_from_kernel(plane.vertices[1]),
            vec3_from_kernel(plane.vertices[2]),
            vec3_from_kernel(plane.vertices[3]),
        ],
    }
}

unsafe fn axes_from_kernel(
    raw: &KernelSymmetryCalculationResult,
) -> Result<Vec<RotationAxis>, String> {
    if raw.axis_count == 0 {
        return Ok(Vec::new());
    }
    if raw.axes.is_null() {
        return Err("kernel symmetry calculation returned null axes".to_string());
    }
    Ok(slice::from_raw_parts(raw.axes, raw.axis_count)
        .iter()
        .map(axis_from_kernel)
        .collect())
}

unsafe fn planes_from_kernel(
    raw: &KernelSymmetryCalculationResult,
) -> Result<Vec<SymmetryPlane>, String> {
    if raw.plane_count == 0 {
        return Ok(Vec::new());
    }
    if raw.planes.is_null() {
        return Err("kernel symmetry calculation returned null planes".to_string());
    }
    Ok(slice::from_raw_parts(raw.planes, raw.plane_count)
        .iter()
        .map(plane_from_kernel)
        .collect())
}

unsafe fn symmetry_data_from_kernel(
    raw: &KernelSymmetryCalculationResult,
) -> Result<SymmetryData, String> {
    let point_group = CStr::from_ptr(raw.point_group.as_ptr())
        .to_str()
        .map_err(|_| "kernel symmetry calculation returned invalid UTF-8 point group".to_string())?
        .to_string();
    Ok(SymmetryData {
        point_group,
        planes: planes_from_kernel(raw)?,
        axes: axes_from_kernel(raw)?,
        has_inversion: raw.has_inversion != 0,
        atom_count: raw.atom_count,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_water_c2v() {
        let xyz =
            "3\nwater\nO  0.000  0.000  0.117\nH  0.000  0.757 -0.469\nH  0.000 -0.757 -0.469\n";
        let result = calculate(xyz, "xyz").unwrap();
        assert_eq!(result.point_group, "C_2v");
        assert!(!result.axes.is_empty());
        assert!(!result.planes.is_empty());
    }

    #[test]
    fn test_co2_linear() {
        let xyz =
            "3\nCO2\nC  0.000  0.000  0.000\nO  0.000  0.000  1.160\nO  0.000  0.000 -1.160\n";
        let result = calculate(xyz, "xyz").unwrap();
        assert_eq!(result.point_group, "D∞h");
        assert!(result.has_inversion);
    }

    #[test]
    fn test_single_atom() {
        let xyz = "1\nHe\nHe  0.0  0.0  0.0\n";
        let result = calculate(xyz, "xyz").unwrap();
        assert_eq!(result.point_group, "K_h");
    }
}
