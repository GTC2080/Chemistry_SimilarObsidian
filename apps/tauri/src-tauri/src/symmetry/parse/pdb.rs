use nalgebra::Vector3;

use crate::symmetry::elements::{atomic_mass, normalize_element};
use crate::symmetry::types::Atom;

pub(super) fn parse_pdb(raw: &str) -> Result<Vec<Atom>, String> {
    let mut atoms = Vec::new();
    for line in raw.lines() {
        if line.starts_with("ATOM") || line.starts_with("HETATM") {
            if line.len() < 54 {
                continue;
            }
            let x = line[30..38]
                .trim()
                .parse::<f64>()
                .map_err(|_| "PDB 坐标解析失败")?;
            let y = line[38..46]
                .trim()
                .parse::<f64>()
                .map_err(|_| "PDB 坐标解析失败")?;
            let z = line[46..54]
                .trim()
                .parse::<f64>()
                .map_err(|_| "PDB 坐标解析失败")?;

            let element = if line.len() >= 78 {
                line[76..78].trim().to_string()
            } else {
                let atom_name = line[12..16].trim();
                atom_name
                    .chars()
                    .filter(|c| c.is_alphabetic())
                    .take(2)
                    .collect::<String>()
            };
            let element = normalize_element(&element);
            let mass = atomic_mass(&element);
            atoms.push(Atom {
                element,
                pos: Vector3::new(x, y, z),
                mass,
            });
        }
    }
    Ok(atoms)
}
