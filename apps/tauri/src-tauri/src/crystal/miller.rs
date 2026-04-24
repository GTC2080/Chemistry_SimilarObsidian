use super::types::{CellParams, MillerPlaneData};

/// 根据密勒指数 (h, k, l) 和晶格基矢计算切割面
///
/// 平面方程：hx/a + ky/b + lz/c = 1（在倒格矢空间中）
/// 转换到笛卡尔坐标后返回法向量、中心点、D 值和四个可视化顶点
pub(super) fn calculate_miller_plane(
    cell: &CellParams,
    h: i32,
    k: i32,
    l: i32,
) -> Result<MillerPlaneData, String> {
    if h == 0 && k == 0 && l == 0 {
        return Err("密勒指数 (h, k, l) 不能全为零".into());
    }

    let vecs = cell.lattice_vectors()?;
    let [va, vb, vc] = vecs;

    // 倒格矢计算: b_i = (v_j × v_k) / (v_i · (v_j × v_k))
    let vol = dot(va, cross(vb, vc));
    if vol.abs() < 1e-12 {
        return Err("晶胞体积为零，无法计算密勒面".into());
    }

    let recip_a = scale(cross(vb, vc), 1.0 / vol);
    let recip_b = scale(cross(vc, va), 1.0 / vol);
    let recip_c = scale(cross(va, vb), 1.0 / vol);

    // 密勒面法向量 = h*a* + k*b* + l*c*
    let normal = [
        h as f64 * recip_a[0] + k as f64 * recip_b[0] + l as f64 * recip_c[0],
        h as f64 * recip_a[1] + k as f64 * recip_b[1] + l as f64 * recip_c[1],
        h as f64 * recip_a[2] + k as f64 * recip_b[2] + l as f64 * recip_c[2],
    ];
    let norm_len = (normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]).sqrt();
    if norm_len < 1e-12 {
        return Err("法向量长度为零".into());
    }
    let unit_normal = [
        normal[0] / norm_len,
        normal[1] / norm_len,
        normal[2] / norm_len,
    ];

    // 平面到原点的距离 d = 1 / |G_hkl| = 面间距
    let d_spacing = 1.0 / norm_len;

    // 平面方程: n · r = d_spacing  → Ax + By + Cz + D = 0, D = -d_spacing
    let d_eq = -d_spacing;

    // 中心点：在法向量方向上距原点 d_spacing
    let center = [
        unit_normal[0] * d_spacing,
        unit_normal[1] * d_spacing,
        unit_normal[2] * d_spacing,
    ];

    // 构造平面上的两个正交向量用于渲染可视化矩形
    let vertices = build_plane_vertices(unit_normal, center, cell);

    Ok(MillerPlaneData {
        normal: unit_normal,
        center,
        d: d_eq,
        vertices,
    })
}

/// 在平面上构造一个可视化矩形（4 个顶点）
fn build_plane_vertices(normal: [f64; 3], center: [f64; 3], cell: &CellParams) -> [[f64; 3]; 4] {
    // 平面半径：取晶胞最长边 × 1.2
    let radius = cell.a.max(cell.b).max(cell.c) * 1.2;

    // 找一个不与法向量平行的参考向量
    let ref_vec = if normal[0].abs() < 0.9 {
        [1.0, 0.0, 0.0]
    } else {
        [0.0, 1.0, 0.0]
    };

    // u = normal × ref (平面内方向1)
    let u_raw = cross(normal, ref_vec);
    let u_len = (u_raw[0] * u_raw[0] + u_raw[1] * u_raw[1] + u_raw[2] * u_raw[2]).sqrt();
    let u = [u_raw[0] / u_len, u_raw[1] / u_len, u_raw[2] / u_len];

    // v = normal × u (平面内方向2)
    let v = cross(normal, u);

    [
        [
            center[0] - radius * u[0] - radius * v[0],
            center[1] - radius * u[1] - radius * v[1],
            center[2] - radius * u[2] - radius * v[2],
        ],
        [
            center[0] + radius * u[0] - radius * v[0],
            center[1] + radius * u[1] - radius * v[1],
            center[2] + radius * u[2] - radius * v[2],
        ],
        [
            center[0] + radius * u[0] + radius * v[0],
            center[1] + radius * u[1] + radius * v[1],
            center[2] + radius * u[2] + radius * v[2],
        ],
        [
            center[0] - radius * u[0] + radius * v[0],
            center[1] - radius * u[1] + radius * v[1],
            center[2] - radius * u[2] + radius * v[2],
        ],
    ]
}

fn cross(a: [f64; 3], b: [f64; 3]) -> [f64; 3] {
    [
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    ]
}

fn dot(a: [f64; 3], b: [f64; 3]) -> f64 {
    a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
}

fn scale(v: [f64; 3], s: f64) -> [f64; 3] {
    [v[0] * s, v[1] * s, v[2] * s]
}

#[cfg(test)]
mod tests {
    use super::*;

    fn cubic_cell(a: f64) -> CellParams {
        CellParams {
            a,
            b: a,
            c: a,
            alpha_deg: 90.0,
            beta_deg: 90.0,
            gamma_deg: 90.0,
        }
    }

    #[test]
    fn test_miller_100() {
        let cell = cubic_cell(5.0);
        let plane = calculate_miller_plane(&cell, 1, 0, 0).unwrap();
        // Normal should be along x
        assert!((plane.normal[0] - 1.0).abs() < 1e-6);
        assert!(plane.normal[1].abs() < 1e-6);
        assert!(plane.normal[2].abs() < 1e-6);
        // d_spacing = a/h = 5.0
        assert!((plane.center[0] - 5.0).abs() < 1e-6);
    }

    #[test]
    fn test_miller_111() {
        let cell = cubic_cell(5.0);
        let plane = calculate_miller_plane(&cell, 1, 1, 1).unwrap();
        // Normal should be [1,1,1]/sqrt(3)
        let inv_sqrt3 = 1.0 / 3.0_f64.sqrt();
        assert!((plane.normal[0] - inv_sqrt3).abs() < 1e-6);
        assert!((plane.normal[1] - inv_sqrt3).abs() < 1e-6);
        assert!((plane.normal[2] - inv_sqrt3).abs() < 1e-6);
    }

    #[test]
    fn test_miller_zero_rejected() {
        let cell = cubic_cell(5.0);
        assert!(calculate_miller_plane(&cell, 0, 0, 0).is_err());
    }
}
