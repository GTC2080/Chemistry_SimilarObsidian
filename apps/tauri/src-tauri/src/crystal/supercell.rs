use std::collections::HashSet;

use super::types::{AtomNode, CellParams, FractionalAtom, SymOp};

/// 网格量化精度的倒数（1/OVERLAP_TOL）
const GRID_SCALE: f64 = 50.0; // 1/0.02

/// 最大原子数限制（防止前端 WebGL 崩溃）
const MAX_ATOMS: usize = 50_000;

/// 将分数坐标量化为整数网格 key，用于 O(1) 去重
fn grid_key(element: &str, frac: &[f64; 3]) -> (String, i64, i64, i64) {
    // 先归一化到 [0, 1)，再量化；考虑周期性边界
    let quantize = |v: f64| -> i64 {
        let norm = v.rem_euclid(1.0);
        (norm * GRID_SCALE).round() as i64 % (GRID_SCALE as i64)
    };
    (
        element.to_string(),
        quantize(frac[0]),
        quantize(frac[1]),
        quantize(frac[2]),
    )
}

/// 应用对称操作 → 生成完整单胞原子 → 扩展为超晶胞
pub(super) fn build_supercell(
    cell: &CellParams,
    raw_atoms: &[FractionalAtom],
    symops: &[SymOp],
    nx: u32,
    ny: u32,
    nz: u32,
) -> Result<Vec<AtomNode>, String> {
    let vecs = cell.lattice_vectors()?;

    // Step 1: 应用对称操作补全单胞原子（HashSet O(1) 去重）
    let capacity = raw_atoms.len() * symops.len();
    let mut seen: HashSet<(String, i64, i64, i64)> = HashSet::with_capacity(capacity);
    let mut unit_atoms: Vec<FractionalAtom> = Vec::with_capacity(capacity);

    for atom in raw_atoms {
        for op in symops {
            let mut new_frac = [0.0_f64; 3];
            for i in 0..3 {
                new_frac[i] = op.rot[i][0] * atom.frac[0]
                    + op.rot[i][1] * atom.frac[1]
                    + op.rot[i][2] * atom.frac[2]
                    + op.trans[i];
                new_frac[i] = new_frac[i].rem_euclid(1.0);
            }

            let key = grid_key(&atom.element, &new_frac);
            if seen.insert(key) {
                unit_atoms.push(FractionalAtom {
                    element: atom.element.clone(),
                    frac: new_frac,
                });
            }
        }
    }

    // Step 2: 超晶胞扩展
    let total_estimate = unit_atoms.len() * (nx as usize) * (ny as usize) * (nz as usize);
    if total_estimate > MAX_ATOMS {
        return Err(format!(
            "超晶胞原子数 ({}) 超过上限 ({})，请减小扩展维度",
            total_estimate, MAX_ATOMS
        ));
    }

    let mut result = Vec::with_capacity(total_estimate);

    for ix in 0..nx {
        for iy in 0..ny {
            for iz in 0..nz {
                for atom in &unit_atoms {
                    let shifted_frac = [
                        atom.frac[0] + ix as f64,
                        atom.frac[1] + iy as f64,
                        atom.frac[2] + iz as f64,
                    ];
                    let cart = cell.frac_to_cart(shifted_frac, &vecs);
                    result.push(AtomNode {
                        element: atom.element.clone(),
                        cartesian_coords: cart,
                    });
                }
            }
        }
    }

    Ok(result)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_simple_cubic_supercell() {
        let cell = CellParams {
            a: 3.0,
            b: 3.0,
            c: 3.0,
            alpha_deg: 90.0,
            beta_deg: 90.0,
            gamma_deg: 90.0,
        };
        let atoms = vec![FractionalAtom {
            element: "Na".into(),
            frac: [0.0, 0.0, 0.0],
        }];
        let symops = vec![SymOp {
            rot: [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]],
            trans: [0.0, 0.0, 0.0],
        }];

        let result = build_supercell(&cell, &atoms, &symops, 2, 2, 2).unwrap();
        assert_eq!(result.len(), 8);
        let last = &result[7];
        assert!((last.cartesian_coords[0] - 3.0).abs() < 1e-8);
    }

    #[test]
    fn test_nacl_with_symops() {
        let cell = CellParams {
            a: 5.64,
            b: 5.64,
            c: 5.64,
            alpha_deg: 90.0,
            beta_deg: 90.0,
            gamma_deg: 90.0,
        };
        let atoms = vec![
            FractionalAtom {
                element: "Na".into(),
                frac: [0.0, 0.0, 0.0],
            },
            FractionalAtom {
                element: "Cl".into(),
                frac: [0.5, 0.0, 0.0],
            },
        ];
        let symops = vec![
            SymOp {
                rot: [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]],
                trans: [0.0, 0.0, 0.0],
            },
            SymOp {
                rot: [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]],
                trans: [0.0, 0.5, 0.5],
            },
            SymOp {
                rot: [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]],
                trans: [0.5, 0.0, 0.5],
            },
            SymOp {
                rot: [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]],
                trans: [0.5, 0.5, 0.0],
            },
        ];

        let result = build_supercell(&cell, &atoms, &symops, 1, 1, 1).unwrap();
        assert!(result.len() == 8);
    }

    #[test]
    fn test_dedup_identical_symops() {
        let cell = CellParams {
            a: 3.0,
            b: 3.0,
            c: 3.0,
            alpha_deg: 90.0,
            beta_deg: 90.0,
            gamma_deg: 90.0,
        };
        let atoms = vec![FractionalAtom {
            element: "Fe".into(),
            frac: [0.0, 0.0, 0.0],
        }];
        // Two identical identity ops should produce only 1 atom
        let symops = vec![
            SymOp {
                rot: [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]],
                trans: [0.0, 0.0, 0.0],
            },
            SymOp {
                rot: [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]],
                trans: [0.0, 0.0, 0.0],
            },
        ];
        let result = build_supercell(&cell, &atoms, &symops, 1, 1, 1).unwrap();
        assert_eq!(result.len(), 1);
    }
}
