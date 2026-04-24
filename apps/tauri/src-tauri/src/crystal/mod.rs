//! 晶格解析与超晶胞生成引擎
//!
//! Tauri Rust 保留 CIF 解析与 command-facing DTO，晶体学数值计算逐步桥接到 C++ kernel。

mod miller;
mod parse;
mod supercell;
mod types;

pub use types::{LatticeData, MillerPlaneData, UnitCellBox};

use types::AtomNode;

/// 解析 CIF 文件并构建超晶胞
///
/// - `cif_text`: CIF 文件的纯文本内容
/// - `nx, ny, nz`: 超晶胞扩展维度（1 = 单胞）
pub fn parse_and_build_lattice(
    cif_text: &str,
    nx: u32,
    ny: u32,
    nz: u32,
) -> Result<LatticeData, String> {
    let (cell, raw_atoms, symops) = parse::parse_cif_full(cif_text)?;
    let vecs = cell.lattice_vectors()?;

    let atoms: Vec<AtomNode> = supercell::build_supercell(&cell, &raw_atoms, &symops, nx, ny, nz)?;

    Ok(LatticeData {
        unit_cell: UnitCellBox {
            a: cell.a,
            b: cell.b,
            c: cell.c,
            alpha: cell.alpha_deg,
            beta: cell.beta_deg,
            gamma: cell.gamma_deg,
            origin: [0.0, 0.0, 0.0],
            vectors: vecs,
        },
        atoms,
    })
}

/// 计算密勒指数切割面
///
/// - `cif_text`: CIF 文件文本（用于提取晶胞参数）
/// - `h, k, l`: 密勒指数
pub fn calculate_miller_plane(
    cif_text: &str,
    h: i32,
    k: i32,
    l: i32,
) -> Result<MillerPlaneData, String> {
    let (cell, _, _) = parse::parse_cif_full(cif_text)?;
    miller::calculate_miller_plane(&cell, h, k, l)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_full_pipeline_cubic() {
        let cif = r#"
data_NaCl
_cell_length_a 5.64
_cell_length_b 5.64
_cell_length_c 5.64
_cell_angle_alpha 90
_cell_angle_beta 90
_cell_angle_gamma 90

loop_
_symmetry_equiv_pos_as_xyz
x,y,z

loop_
_atom_site_type_symbol
_atom_site_fract_x
_atom_site_fract_y
_atom_site_fract_z
Na 0.0 0.0 0.0
Cl 0.5 0.5 0.5
"#;
        let data = parse_and_build_lattice(cif, 2, 2, 2).unwrap();
        assert_eq!(data.atoms.len(), 16); // 2 atoms × 2×2×2
        assert!((data.unit_cell.a - 5.64).abs() < 1e-8);

        // Miller plane test
        let plane = calculate_miller_plane(cif, 1, 1, 0).unwrap();
        assert!(plane.normal[2].abs() < 1e-6); // (110) should have no z component
    }
}
