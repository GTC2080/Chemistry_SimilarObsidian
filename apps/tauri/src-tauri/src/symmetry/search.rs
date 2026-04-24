use std::f64::consts::PI;

use nalgebra::Vector3;

use super::geometry::{are_parallel, check_operation, reflect_point, rotate_point};
use super::types::{Atom, FoundAxis, FoundPlane};
use super::TOLERANCE;

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

pub(super) fn find_rotation_axes(atoms: &[Atom], candidates: &[Vector3<f64>]) -> Vec<FoundAxis> {
    let mut axes: Vec<FoundAxis> = Vec::new();

    for dir in candidates {
        for order in [6u8, 5, 4, 3, 2] {
            let angle = 2.0 * PI / (order as f64);
            let is_cn = check_operation(atoms, |v| rotate_point(v, dir, angle));
            if is_cn {
                let already = axes
                    .iter()
                    .any(|a| a.order == order && are_parallel(&a.dir, dir));
                if !already {
                    axes.push(FoundAxis { dir: *dir, order });
                }
            }
        }
    }

    axes.sort_by(|a, b| b.order.cmp(&a.order));
    axes
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
    let mut planes: Vec<FoundPlane> = Vec::new();

    for normal in candidate_normals {
        let is_mirror = check_operation(atoms, |v| reflect_point(v, normal));
        if is_mirror {
            let already = planes.iter().any(|p| are_parallel(&p.normal, normal));
            if !already {
                planes.push(FoundPlane { normal: *normal });
            }
        }
    }

    planes
}
