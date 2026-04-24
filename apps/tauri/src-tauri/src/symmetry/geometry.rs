use nalgebra::{Matrix3, SymmetricEigen, Vector3};

use super::types::Atom;
use super::ANGLE_TOL;

pub(super) fn compute_principal_axes(atoms: &[Atom]) -> [Vector3<f64>; 3] {
    let mut inertia = Matrix3::<f64>::zeros();
    for atom in atoms {
        let r = &atom.pos;
        let m = atom.mass;
        let r2 = r.norm_squared();
        inertia[(0, 0)] += m * (r2 - r.x * r.x);
        inertia[(1, 1)] += m * (r2 - r.y * r.y);
        inertia[(2, 2)] += m * (r2 - r.z * r.z);
        inertia[(0, 1)] -= m * r.x * r.y;
        inertia[(1, 0)] -= m * r.x * r.y;
        inertia[(0, 2)] -= m * r.x * r.z;
        inertia[(2, 0)] -= m * r.x * r.z;
        inertia[(1, 2)] -= m * r.y * r.z;
        inertia[(2, 1)] -= m * r.y * r.z;
    }

    let eigen = SymmetricEigen::new(inertia);
    let vecs = eigen.eigenvectors;
    [
        vecs.column(0).into_owned(),
        vecs.column(1).into_owned(),
        vecs.column(2).into_owned(),
    ]
}

pub(super) fn are_parallel(a: &Vector3<f64>, b: &Vector3<f64>) -> bool {
    a.dot(b).abs() > ANGLE_TOL.cos()
}
