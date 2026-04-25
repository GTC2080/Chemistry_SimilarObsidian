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

/// 单个原子节点（笛卡尔坐标由 kernel 预计算）
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
