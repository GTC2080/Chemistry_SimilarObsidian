use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::slice;

use nalgebra::Vector3;

use super::types::Atom;

const KERNEL_OK: i32 = 0;

#[repr(C)]
#[derive(Debug, Clone, Copy)]
struct KernelStatus {
    code: i32,
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
struct KernelSymmetryAtomRecord {
    element: *mut c_char,
    position: [f64; 3],
    mass: f64,
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
struct KernelSymmetryAtomList {
    atoms: *mut KernelSymmetryAtomRecord,
    count: usize,
    error: i32,
}

impl Default for KernelSymmetryAtomList {
    fn default() -> Self {
        Self {
            atoms: std::ptr::null_mut(),
            count: 0,
            error: 0,
        }
    }
}

extern "C" {
    fn kernel_parse_symmetry_atoms_text(
        raw: *const c_char,
        raw_size: usize,
        format: *const c_char,
        out_atoms: *mut KernelSymmetryAtomList,
    ) -> KernelStatus;
    fn kernel_free_symmetry_atom_list(atoms: *mut KernelSymmetryAtomList);
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

unsafe fn atom_from_kernel(raw: &KernelSymmetryAtomRecord) -> Result<Atom, String> {
    if raw.element.is_null() {
        return Err("kernel symmetry parser returned null element".to_string());
    }
    let element = CStr::from_ptr(raw.element)
        .to_str()
        .map_err(|_| "kernel symmetry parser returned invalid UTF-8 element".to_string())?
        .to_string();
    Ok(Atom {
        element,
        pos: Vector3::new(raw.position[0], raw.position[1], raw.position[2]),
        mass: raw.mass,
    })
}

unsafe fn atoms_from_kernel(raw: &KernelSymmetryAtomList) -> Result<Vec<Atom>, String> {
    if raw.count == 0 {
        return Ok(Vec::new());
    }
    if raw.atoms.is_null() {
        return Err("kernel symmetry parser returned null atom list".to_string());
    }

    let raw_atoms = slice::from_raw_parts(raw.atoms, raw.count);
    raw_atoms
        .iter()
        .map(|atom| atom_from_kernel(atom))
        .collect()
}

pub(super) fn parse_atoms(raw: &str, format: &str) -> Result<Vec<Atom>, String> {
    let c_format = CString::new(format).map_err(|_| format!("不支持的分子文件格式: {}", format))?;
    let mut raw_atoms = KernelSymmetryAtomList::default();
    let status = unsafe {
        kernel_parse_symmetry_atoms_text(
            raw.as_ptr() as *const c_char,
            raw.len(),
            c_format.as_ptr(),
            &mut raw_atoms,
        )
    };

    if status.code != KERNEL_OK {
        let message = parse_error_message(raw_atoms.error, format);
        unsafe { kernel_free_symmetry_atom_list(&mut raw_atoms) };
        return Err(message);
    }

    let atoms = unsafe { atoms_from_kernel(&raw_atoms) };
    unsafe { kernel_free_symmetry_atom_list(&mut raw_atoms) };
    atoms
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parse_atoms_uses_kernel_xyz_parser_and_element_table() {
        let atoms = parse_atoms("2\nx\nFE 0 0 0\nh 0 0 1\n", "xyz").unwrap();

        assert_eq!(atoms.len(), 2);
        assert_eq!(atoms[0].element, "Fe");
        assert!(atoms[0].mass > 55.0);
        assert_eq!(atoms[1].element, "H");
    }

    #[test]
    fn parse_atoms_maps_kernel_errors() {
        let err = parse_atoms("1\n", "xyz").unwrap_err();

        assert_eq!(err, "XYZ 文件格式不完整");
    }
}
