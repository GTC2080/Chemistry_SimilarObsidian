use nalgebra::Vector3;

use crate::symmetry::elements::{atomic_mass, normalize_element};
use crate::symmetry::types::{Atom, CifCell};

pub(super) fn parse_cif_simple(raw: &str) -> Result<Vec<Atom>, String> {
    let mut atoms = Vec::new();
    let lines: Vec<&str> = raw.lines().collect();
    let cell = parse_cif_cell(&lines);
    let mut i = 0;

    while i < lines.len() {
        let line = lines[i].trim();
        if line == "loop_" {
            let mut col_symbol: Option<usize> = None;
            let mut col_x: Option<usize> = None;
            let mut col_y: Option<usize> = None;
            let mut col_z: Option<usize> = None;
            let mut uses_fractional = false;
            let mut col_idx = 0;
            i += 1;

            while i < lines.len() && lines[i].trim().starts_with('_') {
                let field = lines[i].trim();
                if field.contains("type_symbol") || field == "_atom_site_label" {
                    if col_symbol.is_none() {
                        col_symbol = Some(col_idx);
                    }
                }
                if field.contains("_atom_site_fract_x") {
                    col_x = Some(col_idx);
                    uses_fractional = true;
                } else if field.contains("_atom_site_Cartn_x") {
                    col_x = Some(col_idx);
                }
                if field.contains("_atom_site_fract_y") {
                    col_y = Some(col_idx);
                    uses_fractional = true;
                } else if field.contains("_atom_site_Cartn_y") {
                    col_y = Some(col_idx);
                }
                if field.contains("_atom_site_fract_z") {
                    col_z = Some(col_idx);
                    uses_fractional = true;
                } else if field.contains("_atom_site_Cartn_z") {
                    col_z = Some(col_idx);
                }
                col_idx += 1;
                i += 1;
            }

            if let (Some(cs), Some(cx), Some(cy), Some(cz)) = (col_symbol, col_x, col_y, col_z) {
                if uses_fractional && cell.is_none() {
                    return Err(
                        "CIF 使用分数坐标，但缺少完整晶胞参数 (_cell_length_*/_cell_angle_*)"
                            .into(),
                    );
                }
                while i < lines.len() {
                    let data_line = lines[i].trim();
                    if data_line.is_empty()
                        || data_line.starts_with('_')
                        || data_line.starts_with('#')
                        || data_line == "loop_"
                    {
                        break;
                    }
                    let parts: Vec<&str> = data_line.split_whitespace().collect();
                    let max_col = [cs, cx, cy, cz].iter().copied().max().unwrap_or(0);
                    if parts.len() > max_col {
                        let element = normalize_element(parts[cs]);
                        let (x, y, z) = match (
                            parse_cif_number(parts[cx]),
                            parse_cif_number(parts[cy]),
                            parse_cif_number(parts[cz]),
                        ) {
                            (Some(x), Some(y), Some(z)) => {
                                if uses_fractional {
                                    let cart = cell
                                        .as_ref()
                                        .expect(
                                            "fractional CIF coordinates require cell parameters",
                                        )
                                        .frac_to_cart(Vector3::new(x, y, z))?;
                                    (cart.x, cart.y, cart.z)
                                } else {
                                    (x, y, z)
                                }
                            }
                            _ => {
                                i += 1;
                                continue;
                            }
                        };
                        let mass = atomic_mass(&element);
                        atoms.push(Atom {
                            element,
                            pos: Vector3::new(x, y, z),
                            mass,
                        });
                    }
                    i += 1;
                }
            }
        }
        i += 1;
    }
    Ok(atoms)
}

fn parse_cif_number(raw: &str) -> Option<f64> {
    let trimmed = raw.trim().trim_matches('\'').trim_matches('"');
    let core = trimmed.split('(').next().unwrap_or(trimmed);
    core.parse::<f64>().ok()
}

fn parse_cif_cell(lines: &[&str]) -> Option<CifCell> {
    let mut a: Option<f64> = None;
    let mut b: Option<f64> = None;
    let mut c: Option<f64> = None;
    let mut alpha: Option<f64> = None;
    let mut beta: Option<f64> = None;
    let mut gamma: Option<f64> = None;

    for (idx, line) in lines.iter().enumerate() {
        let line = line.trim();
        if line.starts_with("_cell_length_a") {
            a = parse_cif_tag_value(lines, idx, "_cell_length_a");
        } else if line.starts_with("_cell_length_b") {
            b = parse_cif_tag_value(lines, idx, "_cell_length_b");
        } else if line.starts_with("_cell_length_c") {
            c = parse_cif_tag_value(lines, idx, "_cell_length_c");
        } else if line.starts_with("_cell_angle_alpha") {
            alpha = parse_cif_tag_value(lines, idx, "_cell_angle_alpha");
        } else if line.starts_with("_cell_angle_beta") {
            beta = parse_cif_tag_value(lines, idx, "_cell_angle_beta");
        } else if line.starts_with("_cell_angle_gamma") {
            gamma = parse_cif_tag_value(lines, idx, "_cell_angle_gamma");
        }
    }

    match (a, b, c, alpha, beta, gamma) {
        (Some(a), Some(b), Some(c), Some(alpha_deg), Some(beta_deg), Some(gamma_deg)) => {
            Some(CifCell {
                a,
                b,
                c,
                alpha_deg,
                beta_deg,
                gamma_deg,
            })
        }
        _ => None,
    }
}

fn parse_cif_tag_value(lines: &[&str], idx: usize, tag: &str) -> Option<f64> {
    let line = lines.get(idx)?.trim();
    if !line.starts_with(tag) {
        return None;
    }

    let rest = line[tag.len()..].trim();
    if !rest.is_empty() {
        let token = rest.split_whitespace().next().unwrap_or(rest);
        return parse_cif_number(token);
    }

    let mut next_idx = idx + 1;
    while next_idx < lines.len() {
        let next = lines[next_idx].trim();
        if next.is_empty() || next.starts_with('#') {
            next_idx += 1;
            continue;
        }
        if next.starts_with('_') || next.eq_ignore_ascii_case("loop_") || next.starts_with(';') {
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
    fn test_parse_cif_cell_supports_next_line_values() {
        let lines = vec![
            "_cell_length_a",
            "5.000(2)",
            "_cell_length_b  6.000",
            "_cell_length_c",
            "7.000",
            "_cell_angle_alpha",
            "90",
            "_cell_angle_beta 90",
            "_cell_angle_gamma",
            "120.0",
        ];
        let cell = parse_cif_cell(&lines).expect("cell should parse");
        assert!((cell.a - 5.0).abs() < 1e-8);
        assert!((cell.b - 6.0).abs() < 1e-8);
        assert!((cell.c - 7.0).abs() < 1e-8);
        assert!((cell.alpha_deg - 90.0).abs() < 1e-8);
        assert!((cell.beta_deg - 90.0).abs() < 1e-8);
        assert!((cell.gamma_deg - 120.0).abs() < 1e-8);
    }

    #[test]
    fn test_parse_cif_simple_fractional_with_next_line_cell_values() {
        let cif = r#"
data_demo
_cell_length_a
3.0
_cell_length_b
3.0
_cell_length_c
3.0
_cell_angle_alpha
90
_cell_angle_beta
90
_cell_angle_gamma
90
loop_
_atom_site_type_symbol
_atom_site_fract_x
_atom_site_fract_y
_atom_site_fract_z
C 0.0 0.0 0.0
O 0.5 0.0 0.0
"#;

        let atoms = parse_cif_simple(cif).expect("cif should parse");
        assert_eq!(atoms.len(), 2);
        assert!((atoms[0].pos.x - 0.0).abs() < 1e-8);
        assert!((atoms[1].pos.x - 1.5).abs() < 1e-8);
    }
}
