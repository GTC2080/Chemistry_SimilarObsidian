use nalgebra::Vector3;

use super::geometry::find_perpendicular;
use super::types::{RotationAxis, SymmetryPlane, Vec3D};

pub(super) fn build_axis(dir: &Vector3<f64>, order: u8, mol_radius: f64) -> RotationAxis {
    let extend = mol_radius * 1.5;
    let start = -dir * extend;
    let end = dir * extend;

    RotationAxis {
        vector: Vec3D::from_v3(dir),
        center: Vec3D {
            x: 0.0,
            y: 0.0,
            z: 0.0,
        },
        order,
        start: Vec3D::from_v3(&start),
        end: Vec3D::from_v3(&end),
    }
}

pub(super) fn build_plane(normal: &Vector3<f64>, mol_radius: f64) -> SymmetryPlane {
    let size = mol_radius * 1.8;

    let u = find_perpendicular(normal).normalize();
    let v = normal.cross(&u).normalize();

    let vertices = [
        Vec3D::from_v3(&(u * size + v * size)),
        Vec3D::from_v3(&(u * size - v * size)),
        Vec3D::from_v3(&(-u * size - v * size)),
        Vec3D::from_v3(&(-u * size + v * size)),
    ];

    SymmetryPlane {
        normal: Vec3D::from_v3(normal),
        center: Vec3D {
            x: 0.0,
            y: 0.0,
            z: 0.0,
        },
        vertices,
    }
}
