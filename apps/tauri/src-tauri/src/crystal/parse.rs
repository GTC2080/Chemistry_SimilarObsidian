use std::ffi::CStr;
use std::os::raw::c_char;

use super::types::{CellParams, FractionalAtom, SymOp};

const KERNEL_OK: i32 = 0;
const KERNEL_CRYSTAL_PARSE_ERROR_MISSING_CELL: i32 = 1;
const KERNEL_CRYSTAL_PARSE_ERROR_MISSING_ATOMS: i32 = 2;

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
#[derive(Debug)]
struct KernelFractionalAtomRecord {
    element: *mut c_char,
    frac: [f64; 3],
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
struct KernelSymmetryOperationInput {
    rot: [[f64; 3]; 3],
    trans: [f64; 3],
}

#[repr(C)]
#[derive(Debug)]
struct KernelCrystalParseResult {
    cell: KernelCrystalCellParams,
    atoms: *mut KernelFractionalAtomRecord,
    atom_count: usize,
    symops: *mut KernelSymmetryOperationInput,
    symop_count: usize,
    error: i32,
}

impl Default for KernelCrystalParseResult {
    fn default() -> Self {
        Self {
            cell: KernelCrystalCellParams {
                a: 0.0,
                b: 0.0,
                c: 0.0,
                alpha_deg: 0.0,
                beta_deg: 0.0,
                gamma_deg: 0.0,
            },
            atoms: std::ptr::null_mut(),
            atom_count: 0,
            symops: std::ptr::null_mut(),
            symop_count: 0,
            error: 0,
        }
    }
}

extern "C" {
    fn kernel_parse_cif_crystal(
        raw: *const c_char,
        raw_size: usize,
        out_result: *mut KernelCrystalParseResult,
    ) -> KernelStatus;
    fn kernel_free_crystal_parse_result(result: *mut KernelCrystalParseResult);
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

unsafe fn atoms_from_kernel(raw: &KernelCrystalParseResult) -> Result<Vec<FractionalAtom>, String> {
    if raw.atom_count == 0 {
        return Ok(Vec::new());
    }
    if raw.atoms.is_null() {
        return Err("CIF 内核解析结果缺少 atoms".to_string());
    }

    let raw_atoms = std::slice::from_raw_parts(raw.atoms, raw.atom_count);
    let mut atoms = Vec::with_capacity(raw_atoms.len());
    for atom in raw_atoms {
        if atom.element.is_null() {
            return Err("CIF 内核解析结果缺少元素名称".to_string());
        }
        atoms.push(FractionalAtom {
            element: CStr::from_ptr(atom.element).to_string_lossy().into_owned(),
            frac: atom.frac,
        });
    }
    Ok(atoms)
}

unsafe fn symops_from_kernel(raw: &KernelCrystalParseResult) -> Result<Vec<SymOp>, String> {
    if raw.symop_count == 0 {
        return Ok(Vec::new());
    }
    if raw.symops.is_null() {
        return Err("CIF 内核解析结果缺少 symops".to_string());
    }

    let raw_symops = std::slice::from_raw_parts(raw.symops, raw.symop_count);
    Ok(raw_symops
        .iter()
        .map(|op| SymOp {
            rot: op.rot,
            trans: op.trans,
        })
        .collect())
}

unsafe fn parse_result_from_kernel(
    raw: &KernelCrystalParseResult,
) -> Result<(CellParams, Vec<FractionalAtom>, Vec<SymOp>), String> {
    let cell = CellParams {
        a: raw.cell.a,
        b: raw.cell.b,
        c: raw.cell.c,
        alpha_deg: raw.cell.alpha_deg,
        beta_deg: raw.cell.beta_deg,
        gamma_deg: raw.cell.gamma_deg,
    };
    Ok((cell, atoms_from_kernel(raw)?, symops_from_kernel(raw)?))
}

/// 从 CIF 文本解析晶胞参数、分数坐标原子、对称操作。
///
/// Tauri Rust 保留命令层 DTO；CIF 解析规则由 C++ kernel 提供。
pub(super) fn parse_cif_full(
    raw: &str,
) -> Result<(CellParams, Vec<FractionalAtom>, Vec<SymOp>), String> {
    let mut result = KernelCrystalParseResult::default();
    let status =
        unsafe { kernel_parse_cif_crystal(raw.as_ptr() as *const c_char, raw.len(), &mut result) };
    if status.code != KERNEL_OK {
        let message = parse_error_message(result.error);
        unsafe { kernel_free_crystal_parse_result(&mut result) };
        return Err(message);
    }

    let parsed = unsafe { parse_result_from_kernel(&result) };
    unsafe { kernel_free_crystal_parse_result(&mut result) };
    parsed
}

#[cfg(test)]
mod tests {
    use super::*;

    fn minimal_cif_with_symop(symop: &str) -> String {
        format!(
            r#"
data_test
_cell_length_a 5.0
_cell_length_b 5.0
_cell_length_c 5.0
_cell_angle_alpha 90
_cell_angle_beta 90
_cell_angle_gamma 90

loop_
_symmetry_equiv_pos_as_xyz
{}

loop_
_atom_site_type_symbol
_atom_site_fract_x
_atom_site_fract_y
_atom_site_fract_z
Na 0.0 0.0 0.0
"#,
            symop
        )
    }

    #[test]
    fn test_parse_symop_identity() {
        let (_, _, symops) = parse_cif_full(&minimal_cif_with_symop("x,y,z")).unwrap();
        let op = &symops[0];
        assert!((op.rot[0][0] - 1.0).abs() < 1e-8);
        assert!((op.rot[1][1] - 1.0).abs() < 1e-8);
        assert!((op.rot[2][2] - 1.0).abs() < 1e-8);
        assert!(op.trans.iter().all(|v| v.abs() < 1e-8));
    }

    #[test]
    fn test_parse_symop_with_fraction() {
        let (_, _, symops) = parse_cif_full(&minimal_cif_with_symop("-x+1/2,y,-z+1/2")).unwrap();
        let op = &symops[0];
        assert!((op.rot[0][0] - (-1.0)).abs() < 1e-8);
        assert!((op.trans[0] - 0.5).abs() < 1e-8);
        assert!((op.rot[1][1] - 1.0).abs() < 1e-8);
        assert!((op.rot[2][2] - (-1.0)).abs() < 1e-8);
        assert!((op.trans[2] - 0.5).abs() < 1e-8);
    }

    #[test]
    fn test_parse_cif_full_simple() {
        let cif = r#"
data_test
_cell_length_a 5.0
_cell_length_b 5.0
_cell_length_c 5.0
_cell_angle_alpha 90
_cell_angle_beta 90
_cell_angle_gamma 90

loop_
_symmetry_equiv_pos_as_xyz
x,y,z
-x,-y,-z

loop_
_atom_site_type_symbol
_atom_site_fract_x
_atom_site_fract_y
_atom_site_fract_z
Na 0.0 0.0 0.0
Cl 0.5 0.5 0.5
"#;
        let (cell, atoms, symops) = parse_cif_full(cif).unwrap();
        assert!((cell.a - 5.0).abs() < 1e-8);
        assert_eq!(atoms.len(), 2);
        assert_eq!(symops.len(), 2);
    }
}
