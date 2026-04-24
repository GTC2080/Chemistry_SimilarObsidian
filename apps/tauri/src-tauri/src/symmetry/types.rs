use nalgebra::Vector3;
use serde::Serialize;

#[derive(Serialize, Clone, Debug)]
pub struct Vec3D {
    pub x: f64,
    pub y: f64,
    pub z: f64,
}

#[derive(Serialize, Clone, Debug)]
pub struct SymmetryPlane {
    pub normal: Vec3D,
    pub center: Vec3D,
    pub vertices: [Vec3D; 4],
}

#[derive(Serialize, Clone, Debug)]
pub struct RotationAxis {
    pub vector: Vec3D,
    pub center: Vec3D,
    pub order: u8,
    pub start: Vec3D,
    pub end: Vec3D,
}

#[derive(Serialize, Clone, Debug)]
#[serde(rename_all = "camelCase")]
pub struct SymmetryData {
    pub point_group: String,
    pub planes: Vec<SymmetryPlane>,
    pub axes: Vec<RotationAxis>,
    pub has_inversion: bool,
    pub atom_count: usize,
}

#[derive(Clone, Debug)]
pub(super) struct Atom {
    pub(super) element: String,
    pub(super) pos: Vector3<f64>,
    pub(super) mass: f64,
}

#[derive(Clone, Debug)]
pub(super) struct FoundAxis {
    pub(super) dir: Vector3<f64>, // 单位向量
    pub(super) order: u8,
}

#[derive(Clone, Debug)]
pub(super) struct FoundPlane {
    pub(super) normal: Vector3<f64>, // 单位向量
}
