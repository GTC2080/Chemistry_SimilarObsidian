use std::ffi::CString;
use std::os::raw::c_char;

use nalgebra::Vector3;

use super::geometry::are_parallel;
use super::types::{Atom, FoundAxis, FoundPlane};
use super::TOLERANCE;

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
#[derive(Debug, Clone, Copy)]
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
    let mut dirs: Vec<Vector3<f64>> = Vec::new();

    for ax in principal_axes {
        if ax.norm() > 1e-10 {
            add_unique_dir(&mut dirs, ax.normalize());
        }
    }

    add_unique_dir(&mut dirs, Vector3::x());
    add_unique_dir(&mut dirs, Vector3::y());
    add_unique_dir(&mut dirs, Vector3::z());

    for atom in atoms {
        if atom.pos.norm() > TOLERANCE {
            add_unique_dir(&mut dirs, atom.pos.normalize());
        }
    }

    for i in 0..atoms.len() {
        for j in (i + 1)..atoms.len() {
            if atoms[i].element != atoms[j].element {
                continue;
            }
            let mid = (atoms[i].pos + atoms[j].pos) / 2.0;
            if mid.norm() > TOLERANCE * 0.5 {
                add_unique_dir(&mut dirs, mid.normalize());
            }
            let diff = atoms[i].pos - atoms[j].pos;
            if diff.norm() > TOLERANCE {
                add_unique_dir(&mut dirs, diff.normalize());
            }
        }
    }

    let atom_dirs: Vec<Vector3<f64>> = atoms
        .iter()
        .filter(|a| a.pos.norm() > TOLERANCE)
        .map(|a| a.pos.normalize())
        .collect();
    let cross_limit = atom_dirs.len().min(20);
    for i in 0..cross_limit {
        for j in (i + 1)..cross_limit {
            let c = atom_dirs[i].cross(&atom_dirs[j]);
            if c.norm() > 1e-6 {
                add_unique_dir(&mut dirs, c.normalize());
            }
        }
    }

    dirs
}

fn add_unique_dir(dirs: &mut Vec<Vector3<f64>>, new_dir: Vector3<f64>) {
    for existing in dirs.iter() {
        if are_parallel(existing, &new_dir) {
            return;
        }
    }
    dirs.push(new_dir);
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
    let mut normals: Vec<Vector3<f64>> = Vec::new();

    for axis in found_axes {
        add_unique_dir(&mut normals, axis.dir);
    }

    for ax in principal_axes {
        if ax.norm() > 1e-10 {
            add_unique_dir(&mut normals, ax.normalize());
        }
    }

    add_unique_dir(&mut normals, Vector3::x());
    add_unique_dir(&mut normals, Vector3::y());
    add_unique_dir(&mut normals, Vector3::z());

    for atom in atoms {
        if atom.pos.norm() > TOLERANCE {
            add_unique_dir(&mut normals, atom.pos.normalize());
        }
    }

    for i in 0..atoms.len() {
        for j in (i + 1)..atoms.len() {
            if atoms[i].element != atoms[j].element {
                continue;
            }
            let diff = atoms[i].pos - atoms[j].pos;
            if diff.norm() > TOLERANCE {
                add_unique_dir(&mut normals, diff.normalize());
            }
            let mid = (atoms[i].pos + atoms[j].pos) / 2.0;
            if mid.norm() > TOLERANCE * 0.5 {
                add_unique_dir(&mut normals, mid.normalize());
            }
        }
    }

    for axis in found_axes {
        for atom in atoms {
            if atom.pos.norm() > TOLERANCE {
                let c = axis.dir.cross(&atom.pos);
                if c.norm() > 1e-6 {
                    add_unique_dir(&mut normals, c.normalize());
                }
            }
        }
    }

    normals
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
