use nalgebra::Vector3;
use serde::Serialize;

#[derive(Serialize, Clone, Debug)]
pub struct Vec3D {
    pub x: f64,
    pub y: f64,
    pub z: f64,
}

impl Vec3D {
    pub(super) fn from_v3(v: &Vector3<f64>) -> Self {
        Vec3D {
            x: v.x,
            y: v.y,
            z: v.z,
        }
    }
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

#[derive(Clone, Debug)]
pub(super) struct CifCell {
    pub(super) a: f64,
    pub(super) b: f64,
    pub(super) c: f64,
    pub(super) alpha_deg: f64,
    pub(super) beta_deg: f64,
    pub(super) gamma_deg: f64,
}

impl CifCell {
    pub(super) fn frac_to_cart(&self, frac: Vector3<f64>) -> Result<Vector3<f64>, String> {
        let alpha = self.alpha_deg.to_radians();
        let beta = self.beta_deg.to_radians();
        let gamma = self.gamma_deg.to_radians();

        let cos_alpha = alpha.cos();
        let cos_beta = beta.cos();
        let cos_gamma = gamma.cos();
        let sin_gamma = gamma.sin();

        if sin_gamma.abs() < 1e-8 {
            return Err("CIF 晶胞参数非法：gamma 角过小，无法进行分数坐标转换".into());
        }

        let ax = self.a;
        let ay = 0.0;
        let az = 0.0;

        let bx = self.b * cos_gamma;
        let by = self.b * sin_gamma;
        let bz = 0.0;

        let cx = self.c * cos_beta;
        let cy = self.c * (cos_alpha - cos_beta * cos_gamma) / sin_gamma;
        let cz2 = self.c * self.c - cx * cx - cy * cy;
        if cz2 < -1e-8 {
            return Err("CIF 晶胞参数非法：无法构造有效的晶胞基矢".into());
        }
        let cz = cz2.max(0.0).sqrt();

        Ok(Vector3::new(
            frac.x * ax + frac.y * bx + frac.z * cx,
            frac.x * ay + frac.y * by + frac.z * cy,
            frac.x * az + frac.y * bz + frac.z * cz,
        ))
    }
}
