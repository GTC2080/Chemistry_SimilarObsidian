use std::ffi::CString;
use std::os::raw::c_char;

use nalgebra::Vector3;

use super::types::{Atom, FoundAxis, FoundPlane};

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
struct KernelSymmetryDirectionInput {
    dir: [f64; 3],
}

#[repr(C)]
#[derive(Debug, Clone, Copy, Default)]
struct KernelSymmetryAxisInput {
    dir: [f64; 3],
    order: u8,
}

#[repr(C)]
#[derive(Debug, Clone, Copy, Default)]
struct KernelSymmetryPlaneInput {
    normal: [f64; 3],
}

extern "C" {
    fn kernel_generate_symmetry_candidate_directions(
        atoms: *const KernelSymmetryAtomInput,
        atom_count: usize,
        principal_axes: *const KernelSymmetryDirectionInput,
        principal_axis_count: usize,
        out_directions: *mut KernelSymmetryDirectionInput,
        out_direction_capacity: usize,
        out_direction_count: *mut usize,
    ) -> KernelStatus;
    fn kernel_generate_symmetry_candidate_planes(
        atoms: *const KernelSymmetryAtomInput,
        atom_count: usize,
        found_axes: *const KernelSymmetryAxisInput,
        axis_count: usize,
        principal_axes: *const KernelSymmetryDirectionInput,
        principal_axis_count: usize,
        out_planes: *mut KernelSymmetryPlaneInput,
        out_plane_capacity: usize,
        out_plane_count: *mut usize,
    ) -> KernelStatus;
    fn kernel_find_symmetry_rotation_axes(
        atoms: *const KernelSymmetryAtomInput,
        atom_count: usize,
        candidates: *const KernelSymmetryDirectionInput,
        candidate_count: usize,
        out_axes: *mut KernelSymmetryAxisInput,
        out_axis_capacity: usize,
        out_axis_count: *mut usize,
    ) -> KernelStatus;
    fn kernel_find_symmetry_mirror_planes(
        atoms: *const KernelSymmetryAtomInput,
        atom_count: usize,
        candidates: *const KernelSymmetryPlaneInput,
        candidate_count: usize,
        out_planes: *mut KernelSymmetryPlaneInput,
        out_plane_capacity: usize,
        out_plane_count: *mut usize,
    ) -> KernelStatus;
}

pub(super) fn generate_candidate_directions(
    atoms: &[Atom],
    principal_axes: &[Vector3<f64>; 3],
) -> Vec<Vector3<f64>> {
    let Ok((_elements, kernel_atoms)) = build_kernel_atoms(atoms) else {
        return Vec::new();
    };
    let kernel_principal_axes: Vec<KernelSymmetryDirectionInput> =
        principal_axes.iter().map(direction_to_kernel).collect();
    let mut out_directions =
        vec![KernelSymmetryDirectionInput::default(); direction_candidate_capacity(atoms.len())];
    let mut out_count = 0usize;

    let status = unsafe {
        kernel_generate_symmetry_candidate_directions(
            kernel_atoms.as_ptr(),
            kernel_atoms.len(),
            kernel_principal_axes.as_ptr(),
            kernel_principal_axes.len(),
            out_directions.as_mut_ptr(),
            out_directions.len(),
            &mut out_count,
        )
    };
    if status.code != KERNEL_OK {
        return Vec::new();
    }

    out_directions
        .iter()
        .take(out_count)
        .map(direction_from_kernel)
        .collect()
}

fn build_kernel_atoms(
    atoms: &[Atom],
) -> Result<(Vec<CString>, Vec<KernelSymmetryAtomInput>), String> {
    let elements: Vec<CString> = atoms
        .iter()
        .map(|atom| {
            CString::new(atom.element.as_str()).map_err(|_| {
                "kernel symmetry operation search received invalid element".to_string()
            })
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

    Ok((elements, kernel_atoms))
}

fn direction_to_kernel(direction: &Vector3<f64>) -> KernelSymmetryDirectionInput {
    KernelSymmetryDirectionInput {
        dir: [direction.x, direction.y, direction.z],
    }
}

fn plane_to_kernel(normal: &Vector3<f64>) -> KernelSymmetryPlaneInput {
    KernelSymmetryPlaneInput {
        normal: [normal.x, normal.y, normal.z],
    }
}

fn direction_from_kernel(direction: &KernelSymmetryDirectionInput) -> Vector3<f64> {
    Vector3::new(direction.dir[0], direction.dir[1], direction.dir[2])
}

fn axis_to_kernel(axis: &FoundAxis) -> KernelSymmetryAxisInput {
    KernelSymmetryAxisInput {
        dir: [axis.dir.x, axis.dir.y, axis.dir.z],
        order: axis.order,
    }
}

fn axis_from_kernel(axis: &KernelSymmetryAxisInput) -> FoundAxis {
    FoundAxis {
        dir: Vector3::new(axis.dir[0], axis.dir[1], axis.dir[2]),
        order: axis.order,
    }
}

fn plane_from_kernel(plane: &KernelSymmetryPlaneInput) -> FoundPlane {
    FoundPlane {
        normal: Vector3::new(plane.normal[0], plane.normal[1], plane.normal[2]),
    }
}

fn direction_candidate_capacity(atom_count: usize) -> usize {
    6usize
        .saturating_add(atom_count)
        .saturating_add(atom_count.saturating_mul(atom_count.saturating_sub(1)))
        .saturating_add(190)
        .max(1)
}

fn plane_candidate_capacity(atom_count: usize, axis_count: usize) -> usize {
    axis_count
        .saturating_add(6)
        .saturating_add(atom_count)
        .saturating_add(atom_count.saturating_mul(atom_count.saturating_sub(1)))
        .saturating_add(axis_count.saturating_mul(atom_count))
        .max(1)
}

pub(super) fn find_rotation_axes(atoms: &[Atom], candidates: &[Vector3<f64>]) -> Vec<FoundAxis> {
    let Ok((_elements, kernel_atoms)) = build_kernel_atoms(atoms) else {
        return Vec::new();
    };
    let kernel_candidates: Vec<KernelSymmetryDirectionInput> =
        candidates.iter().map(direction_to_kernel).collect();
    let mut out_axes = vec![KernelSymmetryAxisInput::default(); kernel_candidates.len() * 5];
    let mut out_count = 0usize;

    let status = unsafe {
        kernel_find_symmetry_rotation_axes(
            kernel_atoms.as_ptr(),
            kernel_atoms.len(),
            kernel_candidates.as_ptr(),
            kernel_candidates.len(),
            out_axes.as_mut_ptr(),
            out_axes.len(),
            &mut out_count,
        )
    };
    if status.code != KERNEL_OK {
        return Vec::new();
    }

    out_axes
        .iter()
        .take(out_count)
        .map(axis_from_kernel)
        .collect()
}

pub(super) fn generate_candidate_planes(
    atoms: &[Atom],
    found_axes: &[FoundAxis],
    principal_axes: &[Vector3<f64>; 3],
) -> Vec<Vector3<f64>> {
    let Ok((_elements, kernel_atoms)) = build_kernel_atoms(atoms) else {
        return Vec::new();
    };
    let kernel_axes: Vec<KernelSymmetryAxisInput> = found_axes.iter().map(axis_to_kernel).collect();
    let kernel_principal_axes: Vec<KernelSymmetryDirectionInput> =
        principal_axes.iter().map(direction_to_kernel).collect();
    let mut out_planes = vec![
        KernelSymmetryPlaneInput::default();
        plane_candidate_capacity(atoms.len(), found_axes.len())
    ];
    let mut out_count = 0usize;

    let status = unsafe {
        kernel_generate_symmetry_candidate_planes(
            kernel_atoms.as_ptr(),
            kernel_atoms.len(),
            kernel_axes.as_ptr(),
            kernel_axes.len(),
            kernel_principal_axes.as_ptr(),
            kernel_principal_axes.len(),
            out_planes.as_mut_ptr(),
            out_planes.len(),
            &mut out_count,
        )
    };
    if status.code != KERNEL_OK {
        return Vec::new();
    }

    out_planes
        .iter()
        .take(out_count)
        .map(|plane| Vector3::new(plane.normal[0], plane.normal[1], plane.normal[2]))
        .collect()
}

pub(super) fn find_mirror_planes(
    atoms: &[Atom],
    candidate_normals: &[Vector3<f64>],
) -> Vec<FoundPlane> {
    let Ok((_elements, kernel_atoms)) = build_kernel_atoms(atoms) else {
        return Vec::new();
    };
    let kernel_candidates: Vec<KernelSymmetryPlaneInput> =
        candidate_normals.iter().map(plane_to_kernel).collect();
    let mut out_planes = vec![KernelSymmetryPlaneInput::default(); kernel_candidates.len()];
    let mut out_count = 0usize;

    let status = unsafe {
        kernel_find_symmetry_mirror_planes(
            kernel_atoms.as_ptr(),
            kernel_atoms.len(),
            kernel_candidates.as_ptr(),
            kernel_candidates.len(),
            out_planes.as_mut_ptr(),
            out_planes.len(),
            &mut out_count,
        )
    };
    if status.code != KERNEL_OK {
        return Vec::new();
    }

    out_planes
        .iter()
        .take(out_count)
        .map(plane_from_kernel)
        .collect()
}
