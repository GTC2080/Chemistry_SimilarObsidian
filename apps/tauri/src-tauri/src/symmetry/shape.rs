use std::ffi::CString;
use std::os::raw::c_char;

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
struct KernelSymmetryAtomInput {
    element: *const c_char,
    position: [f64; 3],
    mass: f64,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, Default)]
struct KernelSymmetryShapeResult {
    center_of_mass: [f64; 3],
    mol_radius: f64,
    is_linear: u8,
    linear_axis: [f64; 3],
    has_inversion: u8,
}

extern "C" {
    fn kernel_analyze_symmetry_shape(
        atoms: *const KernelSymmetryAtomInput,
        atom_count: usize,
        out_result: *mut KernelSymmetryShapeResult,
    ) -> KernelStatus;
}

pub(super) struct ShapeAnalysis {
    pub(super) center_of_mass: Vector3<f64>,
    pub(super) mol_radius: f64,
    pub(super) is_linear: bool,
    pub(super) linear_axis: Vector3<f64>,
    pub(super) has_inversion: bool,
}

fn vec3_from_kernel(value: [f64; 3]) -> Vector3<f64> {
    Vector3::new(value[0], value[1], value[2])
}

pub(super) fn analyze_shape(atoms: &[Atom]) -> Result<ShapeAnalysis, String> {
    if atoms.is_empty() {
        return Err("未找到任何原子坐标".to_string());
    }

    let elements: Vec<CString> = atoms
        .iter()
        .map(|atom| {
            CString::new(atom.element.as_str())
                .map_err(|_| "kernel symmetry shape analyzer received invalid element".to_string())
        })
        .collect::<Result<_, _>>()?;
    let kernel_atoms: Vec<KernelSymmetryAtomInput> = atoms
        .iter()
        .zip(elements.iter())
        .map(|(atom, element)| KernelSymmetryAtomInput {
            element: element.as_ptr(),
            position: [atom.pos.x, atom.pos.y, atom.pos.z],
            mass: atom.mass,
        })
        .collect();
    let mut raw_result = KernelSymmetryShapeResult::default();

    let status = unsafe {
        kernel_analyze_symmetry_shape(kernel_atoms.as_ptr(), kernel_atoms.len(), &mut raw_result)
    };
    if status.code != KERNEL_OK {
        return Err("对称性形状分析失败".to_string());
    }

    Ok(ShapeAnalysis {
        center_of_mass: vec3_from_kernel(raw_result.center_of_mass),
        mol_radius: raw_result.mol_radius,
        is_linear: raw_result.is_linear != 0,
        linear_axis: vec3_from_kernel(raw_result.linear_axis),
        has_inversion: raw_result.has_inversion != 0,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn analyze_shape_uses_kernel_for_linear_inversion() {
        let atoms = vec![
            Atom {
                element: "C".to_string(),
                pos: Vector3::new(0.0, 0.0, 0.0),
                mass: 12.0,
            },
            Atom {
                element: "O".to_string(),
                pos: Vector3::new(0.0, 0.0, 1.16),
                mass: 16.0,
            },
            Atom {
                element: "O".to_string(),
                pos: Vector3::new(0.0, 0.0, -1.16),
                mass: 16.0,
            },
        ];

        let shape = analyze_shape(&atoms).unwrap();

        assert!(shape.is_linear);
        assert!(shape.has_inversion);
        assert!((shape.linear_axis.z - 1.0).abs() < 1.0e-8);
        assert!((shape.mol_radius - 1.16).abs() < 1.0e-8);
    }
}
