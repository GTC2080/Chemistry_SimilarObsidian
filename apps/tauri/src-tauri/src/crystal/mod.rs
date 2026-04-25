//! 晶格计算命令桥接。
//!
//! CIF 解析、超晶胞构建、晶胞基矢和密勒面几何都由 C++ kernel full-result ABI 提供；
//! Tauri Rust 只保留 command-facing DTO、错误文案和 kernel-owned 结果释放。

mod types;

pub use types::{AtomNode, LatticeData, MillerPlaneData, UnitCellBox};

use std::ffi::CStr;
use std::os::raw::c_char;

const KERNEL_OK: i32 = 0;
const KERNEL_CRYSTAL_PARSE_ERROR_MISSING_CELL: i32 = 1;
const KERNEL_CRYSTAL_PARSE_ERROR_MISSING_ATOMS: i32 = 2;
const KERNEL_CRYSTAL_MILLER_ERROR_ZERO_INDEX: i32 = 1;
const KERNEL_CRYSTAL_MILLER_ERROR_GAMMA_TOO_SMALL: i32 = 2;
const KERNEL_CRYSTAL_MILLER_ERROR_INVALID_BASIS: i32 = 3;
const KERNEL_CRYSTAL_MILLER_ERROR_ZERO_VOLUME: i32 = 4;
const KERNEL_CRYSTAL_MILLER_ERROR_ZERO_NORMAL: i32 = 5;
const KERNEL_CRYSTAL_SUPERCELL_ERROR_GAMMA_TOO_SMALL: i32 = 1;
const KERNEL_CRYSTAL_SUPERCELL_ERROR_INVALID_BASIS: i32 = 2;
const KERNEL_CRYSTAL_SUPERCELL_ERROR_TOO_MANY_ATOMS: i32 = 3;
const MAX_ATOMS: u64 = 50_000;

#[repr(C)]
#[derive(Debug, Clone, Copy)]
struct KernelStatus {
    code: i32,
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
struct KernelUnitCellBox {
    a: f64,
    b: f64,
    c: f64,
    alpha_deg: f64,
    beta_deg: f64,
    gamma_deg: f64,
    origin: [f64; 3],
    vectors: [[f64; 3]; 3],
}

#[repr(C)]
#[derive(Debug)]
struct KernelAtomNode {
    element: *mut c_char,
    cartesian_coords: [f64; 3],
}

#[repr(C)]
#[derive(Debug)]
struct KernelLatticeResult {
    unit_cell: KernelUnitCellBox,
    atoms: *mut KernelAtomNode,
    atom_count: usize,
    estimated_count: u64,
    parse_error: i32,
    supercell_error: i32,
}

impl Default for KernelLatticeResult {
    fn default() -> Self {
        Self {
            unit_cell: KernelUnitCellBox {
                a: 0.0,
                b: 0.0,
                c: 0.0,
                alpha_deg: 0.0,
                beta_deg: 0.0,
                gamma_deg: 0.0,
                origin: [0.0; 3],
                vectors: [[0.0; 3]; 3],
            },
            atoms: std::ptr::null_mut(),
            atom_count: 0,
            estimated_count: 0,
            parse_error: 0,
            supercell_error: 0,
        }
    }
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

#[repr(C)]
#[derive(Debug, Default, Clone, Copy)]
struct KernelCifMillerPlaneResult {
    plane: KernelMillerPlaneResult,
    parse_error: i32,
}

extern "C" {
    fn kernel_build_lattice_from_cif(
        raw: *const c_char,
        raw_size: usize,
        nx: u32,
        ny: u32,
        nz: u32,
        out_result: *mut KernelLatticeResult,
    ) -> KernelStatus;
    fn kernel_calculate_miller_plane_from_cif(
        raw: *const c_char,
        raw_size: usize,
        h: i32,
        k: i32,
        l: i32,
        out_result: *mut KernelCifMillerPlaneResult,
    ) -> KernelStatus;
    fn kernel_free_lattice_result(result: *mut KernelLatticeResult);
}

fn parse_error_message(error: i32) -> String {
    match error {
        KERNEL_CRYSTAL_PARSE_ERROR_MISSING_CELL => {
            "CIF 文件缺少完整的晶胞参数 (_cell_length_*/_cell_angle_*)".to_string()
        }
        KERNEL_CRYSTAL_PARSE_ERROR_MISSING_ATOMS => {
            "CIF 文件中未找到分数坐标原子 (_atom_site_fract_*)".to_string()
        }
        _ => "CIF 内核解析失败".to_string(),
    }
}

fn supercell_error_message(error: i32, estimated_count: u64) -> String {
    match error {
        KERNEL_CRYSTAL_SUPERCELL_ERROR_GAMMA_TOO_SMALL => "晶胞参数非法：gamma 角过小".to_string(),
        KERNEL_CRYSTAL_SUPERCELL_ERROR_INVALID_BASIS => {
            "晶胞参数非法：无法构造有效基矢".to_string()
        }
        KERNEL_CRYSTAL_SUPERCELL_ERROR_TOO_MANY_ATOMS => format!(
            "超晶胞原子数 ({}) 超过上限 ({})，请减小扩展维度",
            estimated_count, MAX_ATOMS
        ),
        _ => "超晶胞内核构建失败".to_string(),
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

fn lattice_error_message(result: &KernelLatticeResult) -> String {
    if result.parse_error != 0 {
        return parse_error_message(result.parse_error);
    }
    if result.supercell_error != 0 {
        return supercell_error_message(result.supercell_error, result.estimated_count);
    }
    "晶格内核构建失败".to_string()
}

fn cif_miller_error_message(result: &KernelCifMillerPlaneResult) -> String {
    if result.parse_error != 0 {
        return parse_error_message(result.parse_error);
    }
    miller_error_message(result.plane.error)
}

unsafe fn atoms_from_kernel(raw: &KernelLatticeResult) -> Result<Vec<AtomNode>, String> {
    if raw.atom_count == 0 {
        return Ok(Vec::new());
    }
    if raw.atoms.is_null() {
        return Err("晶格内核缺少 atoms 输出".to_string());
    }

    let raw_atoms = std::slice::from_raw_parts(raw.atoms, raw.atom_count);
    let mut atoms = Vec::with_capacity(raw_atoms.len());
    for atom in raw_atoms {
        if atom.element.is_null() {
            return Err("晶格内核缺少元素输出".to_string());
        }
        atoms.push(AtomNode {
            element: CStr::from_ptr(atom.element).to_string_lossy().into_owned(),
            cartesian_coords: atom.cartesian_coords,
        });
    }
    Ok(atoms)
}

unsafe fn lattice_data_from_kernel(raw: &KernelLatticeResult) -> Result<LatticeData, String> {
    Ok(LatticeData {
        unit_cell: UnitCellBox {
            a: raw.unit_cell.a,
            b: raw.unit_cell.b,
            c: raw.unit_cell.c,
            alpha: raw.unit_cell.alpha_deg,
            beta: raw.unit_cell.beta_deg,
            gamma: raw.unit_cell.gamma_deg,
            origin: raw.unit_cell.origin,
            vectors: raw.unit_cell.vectors,
        },
        atoms: atoms_from_kernel(raw)?,
    })
}

/// 解析 CIF 文件并构建超晶胞。
pub fn parse_and_build_lattice(
    cif_text: &str,
    nx: u32,
    ny: u32,
    nz: u32,
) -> Result<LatticeData, String> {
    let mut result = KernelLatticeResult::default();
    let status = unsafe {
        kernel_build_lattice_from_cif(
            cif_text.as_ptr() as *const c_char,
            cif_text.len(),
            nx,
            ny,
            nz,
            &mut result,
        )
    };
    if status.code != KERNEL_OK {
        let message = lattice_error_message(&result);
        unsafe { kernel_free_lattice_result(&mut result) };
        return Err(message);
    }

    let data = unsafe { lattice_data_from_kernel(&result) };
    unsafe { kernel_free_lattice_result(&mut result) };
    data
}

/// 从 CIF 文件文本计算密勒指数切割面。
pub fn calculate_miller_plane(
    cif_text: &str,
    h: i32,
    k: i32,
    l: i32,
) -> Result<MillerPlaneData, String> {
    let mut result = KernelCifMillerPlaneResult::default();
    let status = unsafe {
        kernel_calculate_miller_plane_from_cif(
            cif_text.as_ptr() as *const c_char,
            cif_text.len(),
            h,
            k,
            l,
            &mut result,
        )
    };
    if status.code != KERNEL_OK {
        return Err(cif_miller_error_message(&result));
    }

    Ok(MillerPlaneData {
        normal: result.plane.normal,
        center: result.plane.center,
        d: result.plane.d,
        vertices: result.plane.vertices,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn crystal_lattice_full_pipeline_uses_kernel_result() {
        let cif = r#"
data_NaCl
_cell_length_a 5.64
_cell_length_b 5.64
_cell_length_c 5.64
_cell_angle_alpha 90
_cell_angle_beta 90
_cell_angle_gamma 90

loop_
_symmetry_equiv_pos_as_xyz
x,y,z

loop_
_atom_site_type_symbol
_atom_site_fract_x
_atom_site_fract_y
_atom_site_fract_z
Na 0.0 0.0 0.0
Cl 0.5 0.5 0.5
"#;
        let data = parse_and_build_lattice(cif, 2, 2, 2).unwrap();
        assert_eq!(data.atoms.len(), 16);
        assert!((data.unit_cell.a - 5.64).abs() < 1e-8);

        let plane = calculate_miller_plane(cif, 1, 1, 0).unwrap();
        assert!(plane.normal[2].abs() < 1e-6);
    }

    #[test]
    fn crystal_lattice_maps_kernel_errors() {
        let cif = r#"
data_test
loop_
_atom_site_type_symbol
_atom_site_fract_x
_atom_site_fract_y
_atom_site_fract_z
Na 0.0 0.0 0.0
"#;

        let err = parse_and_build_lattice(cif, 1, 1, 1).expect_err("missing cell");
        assert!(err.contains("晶胞参数"));
    }
}
