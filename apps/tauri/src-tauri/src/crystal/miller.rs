use super::types::{CellParams, MillerPlaneData};

const KERNEL_OK: i32 = 0;
const KERNEL_CRYSTAL_MILLER_ERROR_ZERO_INDEX: i32 = 1;
const KERNEL_CRYSTAL_MILLER_ERROR_GAMMA_TOO_SMALL: i32 = 2;
const KERNEL_CRYSTAL_MILLER_ERROR_INVALID_BASIS: i32 = 3;
const KERNEL_CRYSTAL_MILLER_ERROR_ZERO_VOLUME: i32 = 4;
const KERNEL_CRYSTAL_MILLER_ERROR_ZERO_NORMAL: i32 = 5;

#[repr(C)]
#[derive(Debug, Clone, Copy)]
struct KernelStatus {
    code: i32,
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
struct KernelCrystalCellParams {
    a: f64,
    b: f64,
    c: f64,
    alpha_deg: f64,
    beta_deg: f64,
    gamma_deg: f64,
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
struct KernelMillerPlaneResult {
    normal: [f64; 3],
    center: [f64; 3],
    d: f64,
    vertices: [[f64; 3]; 4],
    error: i32,
}

impl Default for KernelMillerPlaneResult {
    fn default() -> Self {
        Self {
            normal: [0.0; 3],
            center: [0.0; 3],
            d: 0.0,
            vertices: [[0.0; 3]; 4],
            error: 0,
        }
    }
}

extern "C" {
    fn kernel_calculate_miller_plane(
        cell: *const KernelCrystalCellParams,
        h: i32,
        k: i32,
        l: i32,
        out_result: *mut KernelMillerPlaneResult,
    ) -> KernelStatus;
}

fn kernel_cell_from(cell: &CellParams) -> KernelCrystalCellParams {
    KernelCrystalCellParams {
        a: cell.a,
        b: cell.b,
        c: cell.c,
        alpha_deg: cell.alpha_deg,
        beta_deg: cell.beta_deg,
        gamma_deg: cell.gamma_deg,
    }
}

fn miller_error_message(error: i32) -> String {
    match error {
        KERNEL_CRYSTAL_MILLER_ERROR_ZERO_INDEX => "密勒指数 (h, k, l) 不能全为零".to_string(),
        KERNEL_CRYSTAL_MILLER_ERROR_GAMMA_TOO_SMALL => "晶胞参数非法：gamma 角过小".to_string(),
        KERNEL_CRYSTAL_MILLER_ERROR_INVALID_BASIS => "晶胞参数非法：无法构造有效基矢".to_string(),
        KERNEL_CRYSTAL_MILLER_ERROR_ZERO_VOLUME => "晶胞体积为零，无法计算密勒面".to_string(),
        KERNEL_CRYSTAL_MILLER_ERROR_ZERO_NORMAL => "法向量长度为零".to_string(),
        _ => "密勒面内核计算失败".to_string(),
    }
}

/// 根据密勒指数 (h, k, l) 和晶格基矢计算切割面。
///
/// 平面几何计算由 C++ kernel 负责。
pub(super) fn calculate_miller_plane(
    cell: &CellParams,
    h: i32,
    k: i32,
    l: i32,
) -> Result<MillerPlaneData, String> {
    let kernel_cell = kernel_cell_from(cell);
    let mut result = KernelMillerPlaneResult::default();
    let status = unsafe { kernel_calculate_miller_plane(&kernel_cell, h, k, l, &mut result) };
    if status.code != KERNEL_OK {
        return Err(miller_error_message(result.error));
    }

    Ok(MillerPlaneData {
        normal: result.normal,
        center: result.center,
        d: result.d,
        vertices: result.vertices,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    fn cubic_cell(a: f64) -> CellParams {
        CellParams {
            a,
            b: a,
            c: a,
            alpha_deg: 90.0,
            beta_deg: 90.0,
            gamma_deg: 90.0,
        }
    }

    #[test]
    fn test_miller_100() {
        let cell = cubic_cell(5.0);
        let plane = calculate_miller_plane(&cell, 1, 0, 0).unwrap();
        // Normal should be along x
        assert!((plane.normal[0] - 1.0).abs() < 1e-6);
        assert!(plane.normal[1].abs() < 1e-6);
        assert!(plane.normal[2].abs() < 1e-6);
        // d_spacing = a/h = 5.0
        assert!((plane.center[0] - 5.0).abs() < 1e-6);
    }

    #[test]
    fn test_miller_111() {
        let cell = cubic_cell(5.0);
        let plane = calculate_miller_plane(&cell, 1, 1, 1).unwrap();
        // Normal should be [1,1,1]/sqrt(3)
        let inv_sqrt3 = 1.0 / 3.0_f64.sqrt();
        assert!((plane.normal[0] - inv_sqrt3).abs() < 1e-6);
        assert!((plane.normal[1] - inv_sqrt3).abs() < 1e-6);
        assert!((plane.normal[2] - inv_sqrt3).abs() < 1e-6);
    }

    #[test]
    fn test_miller_zero_rejected() {
        let cell = cubic_cell(5.0);
        assert!(calculate_miller_plane(&cell, 0, 0, 0).is_err());
    }
}
