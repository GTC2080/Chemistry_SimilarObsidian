//! 分子点群与空间对称性推演桥接层
//!
//! Tauri Rust 保留主轴计算和命令 DTO；解析、形状分析、候选生成、操作匹配、点群分类与渲染几何已由 C++ kernel 提供。
//! 前端零计算：几何数据（平面顶点、轴端点）在 host 命令返回前由 kernel 预计算完毕。

mod classify;
mod geometry;
mod parse;
mod render;
mod search;
mod shape;
mod types;

use classify::classify_point_group;
use geometry::compute_principal_axes;
use parse::parse_atoms;
use render::build_render_geometry;
use search::{
    find_mirror_planes, find_rotation_axes, generate_candidate_directions,
    generate_candidate_planes,
};
use shape::analyze_shape;
use types::{FoundAxis, FoundPlane};

pub use types::SymmetryData;

const MAX_ATOMS_FOR_SYMMETRY: usize = 500;

// ===== 公开接口 =====

pub fn calculate(raw_data: &str, format: &str) -> Result<SymmetryData, String> {
    let mut atoms = parse_atoms(raw_data, format)?;
    if atoms.is_empty() {
        return Err("未找到任何原子坐标".into());
    }
    if atoms.len() > MAX_ATOMS_FOR_SYMMETRY {
        return Err(format!(
            "原子数 ({}) 超过对称性分析上限 ({})，请使用较小的分子",
            atoms.len(),
            MAX_ATOMS_FOR_SYMMETRY
        ));
    }

    let atom_count = atoms.len();

    // 单原子特殊处理
    if atom_count == 1 {
        return Ok(SymmetryData {
            point_group: "K_h".into(),
            planes: vec![],
            axes: vec![],
            has_inversion: true,
            atom_count,
        });
    }

    let shape = analyze_shape(&atoms)?;

    // 平移至质心
    let com = shape.center_of_mass;
    for atom in &mut atoms {
        atom.pos -= com;
    }

    // 分子半径（用于渲染尺寸缩放）
    let mol_radius = shape.mol_radius;

    // 检测线性分子
    let is_linear = shape.is_linear;

    if is_linear {
        let has_inv = shape.has_inversion;
        let pg = if has_inv { "D∞h" } else { "C∞v" };

        // 线性分子：主轴沿分子方向
        let found_axes = vec![FoundAxis {
            dir: shape.linear_axis,
            order: 0, // order=0 表示 ∞
        }];
        let (axes_render, _) = build_render_geometry(&found_axes, &[], mol_radius)?;

        return Ok(SymmetryData {
            point_group: pg.into(),
            planes: vec![],
            axes: axes_render,
            has_inversion: has_inv,
            atom_count,
        });
    }

    // 惯性张量 → 主轴
    let principal_axes = compute_principal_axes(&atoms);

    // 穷举对称操作
    let candidate_dirs = generate_candidate_directions(&atoms, &principal_axes);
    let found_axes: Vec<FoundAxis> = find_rotation_axes(&atoms, &candidate_dirs);
    let candidate_planes = generate_candidate_planes(&atoms, &found_axes, &principal_axes);
    let found_planes: Vec<FoundPlane> = find_mirror_planes(&atoms, &candidate_planes);
    let has_inversion = shape.has_inversion;

    // 点群分类
    let point_group = classify_point_group(&found_axes, &found_planes, has_inversion);

    // 构建渲染数据
    let (axes_render, planes_render) =
        build_render_geometry(&found_axes, &found_planes, mol_radius)?;

    Ok(SymmetryData {
        point_group,
        planes: planes_render,
        axes: axes_render,
        has_inversion,
        atom_count,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_water_c2v() {
        let xyz =
            "3\nwater\nO  0.000  0.000  0.117\nH  0.000  0.757 -0.469\nH  0.000 -0.757 -0.469\n";
        let result = calculate(xyz, "xyz").unwrap();
        assert_eq!(result.point_group, "C_2v");
        assert!(!result.axes.is_empty());
        assert!(!result.planes.is_empty());
    }

    #[test]
    fn test_co2_linear() {
        let xyz =
            "3\nCO2\nC  0.000  0.000  0.000\nO  0.000  0.000  1.160\nO  0.000  0.000 -1.160\n";
        let result = calculate(xyz, "xyz").unwrap();
        assert_eq!(result.point_group, "D∞h");
        assert!(result.has_inversion);
    }

    #[test]
    fn test_single_atom() {
        let xyz = "1\nHe\nHe  0.0  0.0  0.0\n";
        let result = calculate(xyz, "xyz").unwrap();
        assert_eq!(result.point_group, "K_h");
    }
}
