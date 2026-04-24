use serde::Serialize;

/// 晶胞包围盒：边长、夹角、基矢
#[derive(Serialize, Clone, Debug)]
#[serde(rename_all = "camelCase")]
pub struct UnitCellBox {
    pub a: f64,
    pub b: f64,
    pub c: f64,
    pub alpha: f64,
    pub beta: f64,
    pub gamma: f64,
    pub origin: [f64; 3],
    pub vectors: [[f64; 3]; 3],
}

/// 单个原子节点（笛卡尔坐标由 Rust 预计算）
#[derive(Serialize, Clone, Debug)]
#[serde(rename_all = "camelCase")]
pub struct AtomNode {
    pub element: String,
    pub cartesian_coords: [f64; 3],
}

/// 完整晶格数据（传给前端的最终 JSON）
#[derive(Serialize, Clone, Debug)]
#[serde(rename_all = "camelCase")]
pub struct LatticeData {
    pub unit_cell: UnitCellBox,
    pub atoms: Vec<AtomNode>,
}

/// 密勒指数切割面参数
#[derive(Serialize, Clone, Debug)]
#[serde(rename_all = "camelCase")]
pub struct MillerPlaneData {
    pub normal: [f64; 3],
    pub center: [f64; 3],
    pub d: f64,
    pub vertices: [[f64; 3]; 4],
}

/// CIF 解析中间态：分数坐标原子
#[derive(Clone, Debug)]
pub(super) struct FractionalAtom {
    pub element: String,
    pub frac: [f64; 3],
}

/// CIF 对称操作
#[derive(Clone, Debug)]
pub(super) struct SymOp {
    pub rot: [[f64; 3]; 3],
    pub trans: [f64; 3],
}

/// CIF 晶胞参数（内部使用）
#[derive(Clone, Debug)]
pub(super) struct CellParams {
    pub a: f64,
    pub b: f64,
    pub c: f64,
    pub alpha_deg: f64,
    pub beta_deg: f64,
    pub gamma_deg: f64,
}

impl CellParams {
    /// 构建晶格基矢矩阵（列向量形式）
    /// 返回 [vec_a, vec_b, vec_c]，每个是笛卡尔坐标 [x, y, z]
    pub fn lattice_vectors(&self) -> Result<[[f64; 3]; 3], String> {
        let alpha = self.alpha_deg.to_radians();
        let beta = self.beta_deg.to_radians();
        let gamma = self.gamma_deg.to_radians();

        let cos_alpha = alpha.cos();
        let cos_beta = beta.cos();
        let cos_gamma = gamma.cos();
        let sin_gamma = gamma.sin();

        if sin_gamma.abs() < 1e-8 {
            return Err("晶胞参数非法：gamma 角过小".into());
        }

        let ax = self.a;
        let bx = self.b * cos_gamma;
        let by = self.b * sin_gamma;
        let cx = self.c * cos_beta;
        let cy = self.c * (cos_alpha - cos_beta * cos_gamma) / sin_gamma;
        let cz2 = self.c * self.c - cx * cx - cy * cy;
        if cz2 < -1e-8 {
            return Err("晶胞参数非法：无法构造有效基矢".into());
        }
        let cz = cz2.max(0.0).sqrt();

        Ok([[ax, 0.0, 0.0], [bx, by, 0.0], [cx, cy, cz]])
    }
}
