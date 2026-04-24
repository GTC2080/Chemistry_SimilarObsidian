use crate::symmetry::elements::normalize_element;

use super::types::{CellParams, FractionalAtom, SymOp};

/// 从 CIF 文本解析晶胞参数、分数坐标原子、对称操作
pub(super) fn parse_cif_full(
    raw: &str,
) -> Result<(CellParams, Vec<FractionalAtom>, Vec<SymOp>), String> {
    let lines: Vec<&str> = raw.lines().collect();

    let cell = parse_cell_params(&lines)?;
    let atoms = parse_atom_sites(&lines)?;
    let symops = parse_symmetry_ops(&lines);

    Ok((cell, atoms, symops))
}

fn parse_cell_params(lines: &[&str]) -> Result<CellParams, String> {
    let mut a: Option<f64> = None;
    let mut b: Option<f64> = None;
    let mut c: Option<f64> = None;
    let mut alpha: Option<f64> = None;
    let mut beta: Option<f64> = None;
    let mut gamma: Option<f64> = None;

    for (idx, line) in lines.iter().enumerate() {
        let line = line.trim();
        if line.starts_with("_cell_length_a") {
            a = parse_tag_value(lines, idx, "_cell_length_a");
        } else if line.starts_with("_cell_length_b") {
            b = parse_tag_value(lines, idx, "_cell_length_b");
        } else if line.starts_with("_cell_length_c") {
            c = parse_tag_value(lines, idx, "_cell_length_c");
        } else if line.starts_with("_cell_angle_alpha") {
            alpha = parse_tag_value(lines, idx, "_cell_angle_alpha");
        } else if line.starts_with("_cell_angle_beta") {
            beta = parse_tag_value(lines, idx, "_cell_angle_beta");
        } else if line.starts_with("_cell_angle_gamma") {
            gamma = parse_tag_value(lines, idx, "_cell_angle_gamma");
        }
    }

    match (a, b, c, alpha, beta, gamma) {
        (Some(a), Some(b), Some(c), Some(al), Some(be), Some(ga)) => Ok(CellParams {
            a,
            b,
            c,
            alpha_deg: al,
            beta_deg: be,
            gamma_deg: ga,
        }),
        _ => Err("CIF 文件缺少完整的晶胞参数 (_cell_length_*/_cell_angle_*)".into()),
    }
}

fn parse_atom_sites(lines: &[&str]) -> Result<Vec<FractionalAtom>, String> {
    let mut atoms = Vec::new();
    let mut i = 0;

    while i < lines.len() {
        let line = lines[i].trim();
        if line == "loop_" {
            let mut col_symbol: Option<usize> = None;
            let mut col_x: Option<usize> = None;
            let mut col_y: Option<usize> = None;
            let mut col_z: Option<usize> = None;
            let mut col_idx = 0;
            let header_start = i + 1;
            i += 1;

            while i < lines.len() && lines[i].trim().starts_with('_') {
                let field = lines[i].trim();
                if (field.contains("type_symbol") || field == "_atom_site_label")
                    && col_symbol.is_none()
                {
                    col_symbol = Some(col_idx);
                }
                if field.contains("_atom_site_fract_x") {
                    col_x = Some(col_idx);
                }
                if field.contains("_atom_site_fract_y") {
                    col_y = Some(col_idx);
                }
                if field.contains("_atom_site_fract_z") {
                    col_z = Some(col_idx);
                }
                col_idx += 1;
                i += 1;
            }

            // Only process if this is an atom_site loop with fractional coords
            let is_atom_loop = (header_start..i).any(|j| {
                let f = lines[j].trim();
                f.starts_with("_atom_site")
            });

            if is_atom_loop {
                if let (Some(cs), Some(cx), Some(cy), Some(cz)) = (col_symbol, col_x, col_y, col_z)
                {
                    while i < lines.len() {
                        let data = lines[i].trim();
                        if data.is_empty()
                            || data.starts_with('_')
                            || data.starts_with('#')
                            || data == "loop_"
                        {
                            break;
                        }
                        let parts: Vec<&str> = data.split_whitespace().collect();
                        let max_col = [cs, cx, cy, cz].iter().copied().max().unwrap_or(0);
                        if parts.len() > max_col {
                            if let (Some(x), Some(y), Some(z)) = (
                                parse_cif_number(parts[cx]),
                                parse_cif_number(parts[cy]),
                                parse_cif_number(parts[cz]),
                            ) {
                                atoms.push(FractionalAtom {
                                    element: normalize_element(parts[cs]),
                                    frac: [x, y, z],
                                });
                            }
                        }
                        i += 1;
                    }
                } else {
                    // Skip non-fractional atom site loops
                    while i < lines.len() {
                        let data = lines[i].trim();
                        if data.is_empty() || data.starts_with('_') || data == "loop_" {
                            break;
                        }
                        i += 1;
                    }
                }
            }
            continue;
        }
        i += 1;
    }

    if atoms.is_empty() {
        return Err("CIF 文件中未找到分数坐标原子 (_atom_site_fract_*)".into());
    }
    Ok(atoms)
}

/// 解析对称操作 (_symmetry_equiv_pos_as_xyz 或 _space_group_symop_operation_xyz)
fn parse_symmetry_ops(lines: &[&str]) -> Vec<SymOp> {
    let mut ops = Vec::new();
    let mut i = 0;

    while i < lines.len() {
        let line = lines[i].trim();
        if line == "loop_" {
            let mut col_xyz: Option<usize> = None;
            let mut col_idx = 0;
            i += 1;

            while i < lines.len() && lines[i].trim().starts_with('_') {
                let field = lines[i].trim();
                if field.contains("_symmetry_equiv_pos_as_xyz")
                    || field.contains("_space_group_symop_operation_xyz")
                {
                    col_xyz = Some(col_idx);
                }
                col_idx += 1;
                i += 1;
            }

            if let Some(cx) = col_xyz {
                while i < lines.len() {
                    let data = lines[i].trim();
                    if data.is_empty()
                        || data.starts_with('_')
                        || data.starts_with('#')
                        || data == "loop_"
                    {
                        break;
                    }
                    // Extract the xyz string (may be quoted)
                    let parts: Vec<&str> = data
                        .splitn(col_idx.max(cx + 1), char::is_whitespace)
                        .collect();
                    if let Some(xyz_raw) = parts.get(cx) {
                        let xyz = xyz_raw.trim().trim_matches('\'').trim_matches('"');
                        if let Some(op) = parse_symop_xyz(xyz) {
                            ops.push(op);
                        }
                    }
                    i += 1;
                }
            }
            continue;
        }
        i += 1;
    }

    // If no symops found, return identity only
    if ops.is_empty() {
        ops.push(SymOp {
            rot: [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]],
            trans: [0.0, 0.0, 0.0],
        });
    }
    ops
}

/// 解析单条对称操作字符串，如 "x,y,z" 或 "-x+1/2,y,-z+1/2"
fn parse_symop_xyz(xyz: &str) -> Option<SymOp> {
    let parts: Vec<&str> = xyz.split(',').collect();
    if parts.len() != 3 {
        return None;
    }

    let mut rot = [[0.0_f64; 3]; 3];
    let mut trans = [0.0_f64; 3];

    for (row, part) in parts.iter().enumerate() {
        let s = part.trim().to_lowercase().replace(' ', "");
        let (r, t) = parse_symop_component(&s)?;
        rot[row] = r;
        trans[row] = t;
    }

    Some(SymOp { rot, trans })
}

/// 解析对称操作的单个分量，如 "-x+1/2"
fn parse_symop_component(s: &str) -> Option<([f64; 3], f64)> {
    let mut coeff = [0.0_f64; 3]; // x, y, z coefficients
    let mut constant = 0.0_f64;

    let mut i = 0;
    let chars: Vec<char> = s.chars().collect();

    while i < chars.len() {
        let sign = if chars[i] == '-' {
            i += 1;
            -1.0
        } else if chars[i] == '+' {
            i += 1;
            1.0
        } else {
            1.0
        };

        if i >= chars.len() {
            break;
        }

        match chars[i] {
            'x' => {
                coeff[0] = sign;
                i += 1;
            }
            'y' => {
                coeff[1] = sign;
                i += 1;
            }
            'z' => {
                coeff[2] = sign;
                i += 1;
            }
            c if c.is_ascii_digit() => {
                // Parse number or fraction
                let start = i;
                while i < chars.len() && (chars[i].is_ascii_digit() || chars[i] == '.') {
                    i += 1;
                }
                let num: f64 = s[start..i].parse().ok()?;

                if i < chars.len() && chars[i] == '/' {
                    i += 1;
                    let den_start = i;
                    while i < chars.len() && chars[i].is_ascii_digit() {
                        i += 1;
                    }
                    let den: f64 = s[den_start..i].parse().ok()?;
                    if den.abs() < 1e-12 {
                        return None;
                    }
                    constant += sign * num / den;
                } else {
                    // Check if followed by x/y/z (coefficient like 2x)
                    if i < chars.len() && matches!(chars[i], 'x' | 'y' | 'z') {
                        let idx = match chars[i] {
                            'x' => 0,
                            'y' => 1,
                            'z' => 2,
                            _ => unreachable!(),
                        };
                        coeff[idx] = sign * num;
                        i += 1;
                    } else {
                        constant += sign * num;
                    }
                }
            }
            _ => {
                i += 1; // skip unknown
            }
        }
    }

    Some((coeff, constant))
}

fn parse_cif_number(raw: &str) -> Option<f64> {
    let trimmed = raw.trim().trim_matches('\'').trim_matches('"');
    let core = trimmed.split('(').next().unwrap_or(trimmed);
    core.parse::<f64>().ok()
}

fn parse_tag_value(lines: &[&str], idx: usize, tag: &str) -> Option<f64> {
    let line = lines.get(idx)?.trim();
    if !line.starts_with(tag) {
        return None;
    }
    let rest = line[tag.len()..].trim();
    if !rest.is_empty() {
        let token = rest.split_whitespace().next().unwrap_or(rest);
        return parse_cif_number(token);
    }
    // Value may be on the next line
    let mut next_idx = idx + 1;
    while next_idx < lines.len() {
        let next = lines[next_idx].trim();
        if next.is_empty() || next.starts_with('#') {
            next_idx += 1;
            continue;
        }
        if next.starts_with('_') || next.eq_ignore_ascii_case("loop_") {
            return None;
        }
        let token = next.split_whitespace().next().unwrap_or(next);
        return parse_cif_number(token);
    }
    None
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_symop_identity() {
        let op = parse_symop_xyz("x,y,z").unwrap();
        assert!((op.rot[0][0] - 1.0).abs() < 1e-8);
        assert!((op.rot[1][1] - 1.0).abs() < 1e-8);
        assert!((op.rot[2][2] - 1.0).abs() < 1e-8);
        assert!(op.trans.iter().all(|v| v.abs() < 1e-8));
    }

    #[test]
    fn test_parse_symop_with_fraction() {
        let op = parse_symop_xyz("-x+1/2,y,-z+1/2").unwrap();
        assert!((op.rot[0][0] - (-1.0)).abs() < 1e-8);
        assert!((op.trans[0] - 0.5).abs() < 1e-8);
        assert!((op.rot[1][1] - 1.0).abs() < 1e-8);
        assert!((op.rot[2][2] - (-1.0)).abs() < 1e-8);
        assert!((op.trans[2] - 0.5).abs() < 1e-8);
    }

    #[test]
    fn test_parse_cif_full_simple() {
        let cif = r#"
data_test
_cell_length_a 5.0
_cell_length_b 5.0
_cell_length_c 5.0
_cell_angle_alpha 90
_cell_angle_beta 90
_cell_angle_gamma 90

loop_
_symmetry_equiv_pos_as_xyz
x,y,z
-x,-y,-z

loop_
_atom_site_type_symbol
_atom_site_fract_x
_atom_site_fract_y
_atom_site_fract_z
Na 0.0 0.0 0.0
Cl 0.5 0.5 0.5
"#;
        let (cell, atoms, symops) = parse_cif_full(cif).unwrap();
        assert!((cell.a - 5.0).abs() < 1e-8);
        assert_eq!(atoms.len(), 2);
        assert_eq!(symops.len(), 2);
    }
}
