use std::ffi::{CStr, CString};
use std::os::raw::c_char;

use super::types::{AtomNode, CellParams, FractionalAtom, SymOp};

const KERNEL_OK: i32 = 0;
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
struct KernelFractionalAtomInput {
    element: *const c_char,
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
struct KernelAtomNode {
    element: *mut c_char,
    cartesian_coords: [f64; 3],
}

#[repr(C)]
#[derive(Debug)]
struct KernelSupercellResult {
    atoms: *mut KernelAtomNode,
    count: usize,
    estimated_count: u64,
    error: i32,
}

impl Default for KernelSupercellResult {
    fn default() -> Self {
        Self {
            atoms: std::ptr::null_mut(),
            count: 0,
            estimated_count: 0,
            error: 0,
        }
    }
}

extern "C" {
    fn kernel_build_supercell(
        cell: *const KernelCrystalCellParams,
        atoms: *const KernelFractionalAtomInput,
        atom_count: usize,
        symops: *const KernelSymmetryOperationInput,
        symop_count: usize,
        nx: u32,
        ny: u32,
        nz: u32,
        out_result: *mut KernelSupercellResult,
    ) -> KernelStatus;
    fn kernel_free_supercell_result(result: *mut KernelSupercellResult);
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

unsafe fn atom_nodes_from_kernel(raw: &KernelSupercellResult) -> Result<Vec<AtomNode>, String> {
    if raw.count == 0 {
        return Ok(Vec::new());
    }
    if raw.atoms.is_null() {
        return Err("超晶胞内核缺少 atoms 输出".to_string());
    }

    let raw_atoms = std::slice::from_raw_parts(raw.atoms, raw.count);
    let mut atoms = Vec::with_capacity(raw_atoms.len());
    for atom in raw_atoms {
        if atom.element.is_null() {
            return Err("超晶胞内核缺少元素输出".to_string());
        }
        atoms.push(AtomNode {
            element: CStr::from_ptr(atom.element).to_string_lossy().into_owned(),
            cartesian_coords: atom.cartesian_coords,
        });
    }
    Ok(atoms)
}

/// 应用对称操作生成完整单胞原子，再扩展为超晶胞。
///
/// CIF 解析仍由 Tauri Rust 完成；对称展开、去重和笛卡尔坐标计算由 C++ kernel 负责。
pub(super) fn build_supercell(
    cell: &CellParams,
    raw_atoms: &[FractionalAtom],
    symops: &[SymOp],
    nx: u32,
    ny: u32,
    nz: u32,
) -> Result<Vec<AtomNode>, String> {
    let element_strings: Vec<CString> = raw_atoms
        .iter()
        .map(|atom| {
            CString::new(atom.element.as_str()).map_err(|_| "元素名称包含非法空字符".to_string())
        })
        .collect::<Result<_, _>>()?;
    let kernel_atoms: Vec<KernelFractionalAtomInput> = raw_atoms
        .iter()
        .zip(element_strings.iter())
        .map(|(atom, element)| KernelFractionalAtomInput {
            element: element.as_ptr(),
            frac: atom.frac,
        })
        .collect();
    let kernel_symops: Vec<KernelSymmetryOperationInput> = symops
        .iter()
        .map(|op| KernelSymmetryOperationInput {
            rot: op.rot,
            trans: op.trans,
        })
        .collect();

    let kernel_cell = kernel_cell_from(cell);
    let mut raw_result = KernelSupercellResult::default();
    let status = unsafe {
        kernel_build_supercell(
            &kernel_cell,
            kernel_atoms.as_ptr(),
            kernel_atoms.len(),
            kernel_symops.as_ptr(),
            kernel_symops.len(),
            nx,
            ny,
            nz,
            &mut raw_result,
        )
    };
    if status.code != KERNEL_OK {
        let message = supercell_error_message(raw_result.error, raw_result.estimated_count);
        unsafe { kernel_free_supercell_result(&mut raw_result) };
        return Err(message);
    }

    let atoms = unsafe { atom_nodes_from_kernel(&raw_result) };
    unsafe { kernel_free_supercell_result(&mut raw_result) };
    atoms
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_simple_cubic_supercell() {
        let cell = CellParams {
            a: 3.0,
            b: 3.0,
            c: 3.0,
            alpha_deg: 90.0,
            beta_deg: 90.0,
            gamma_deg: 90.0,
        };
        let atoms = vec![FractionalAtom {
            element: "Na".into(),
            frac: [0.0, 0.0, 0.0],
        }];
        let symops = vec![SymOp {
            rot: [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]],
            trans: [0.0, 0.0, 0.0],
        }];

        let result = build_supercell(&cell, &atoms, &symops, 2, 2, 2).unwrap();
        assert_eq!(result.len(), 8);
        let last = &result[7];
        assert!((last.cartesian_coords[0] - 3.0).abs() < 1e-8);
    }

    #[test]
    fn test_nacl_with_symops() {
        let cell = CellParams {
            a: 5.64,
            b: 5.64,
            c: 5.64,
            alpha_deg: 90.0,
            beta_deg: 90.0,
            gamma_deg: 90.0,
        };
        let atoms = vec![
            FractionalAtom {
                element: "Na".into(),
                frac: [0.0, 0.0, 0.0],
            },
            FractionalAtom {
                element: "Cl".into(),
                frac: [0.5, 0.0, 0.0],
            },
        ];
        let symops = vec![
            SymOp {
                rot: [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]],
                trans: [0.0, 0.0, 0.0],
            },
            SymOp {
                rot: [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]],
                trans: [0.0, 0.5, 0.5],
            },
            SymOp {
                rot: [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]],
                trans: [0.5, 0.0, 0.5],
            },
            SymOp {
                rot: [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]],
                trans: [0.5, 0.5, 0.0],
            },
        ];

        let result = build_supercell(&cell, &atoms, &symops, 1, 1, 1).unwrap();
        assert!(result.len() == 8);
    }

    #[test]
    fn test_dedup_identical_symops() {
        let cell = CellParams {
            a: 3.0,
            b: 3.0,
            c: 3.0,
            alpha_deg: 90.0,
            beta_deg: 90.0,
            gamma_deg: 90.0,
        };
        let atoms = vec![FractionalAtom {
            element: "Fe".into(),
            frac: [0.0, 0.0, 0.0],
        }];
        // Two identical identity ops should produce only 1 atom
        let symops = vec![
            SymOp {
                rot: [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]],
                trans: [0.0, 0.0, 0.0],
            },
            SymOp {
                rot: [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]],
                trans: [0.0, 0.0, 0.0],
            },
        ];
        let result = build_supercell(&cell, &atoms, &symops, 1, 1, 1).unwrap();
        assert_eq!(result.len(), 1);
    }
}
