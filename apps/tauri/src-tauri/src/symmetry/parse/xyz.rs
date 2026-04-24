use nalgebra::Vector3;

use crate::symmetry::elements::{atomic_mass, normalize_element};
use crate::symmetry::types::Atom;

pub(super) fn parse_xyz(raw: &str) -> Result<Vec<Atom>, String> {
    let mut lines = raw.lines();
    let _count_line = lines.next().ok_or("XYZ 文件为空")?;
    let _comment = lines.next().ok_or("XYZ 文件格式不完整")?;

    let mut atoms = Vec::new();
    for line in lines {
        let line = line.trim();
        if line.is_empty() {
            continue;
        }
        let parts: Vec<&str> = line.split_whitespace().collect();
        if parts.len() < 4 {
            continue;
        }
        let element = normalize_element(parts[0]);
        let x = parts[1].parse::<f64>().map_err(|_| "XYZ 坐标解析失败")?;
        let y = parts[2].parse::<f64>().map_err(|_| "XYZ 坐标解析失败")?;
        let z = parts[3].parse::<f64>().map_err(|_| "XYZ 坐标解析失败")?;
        let mass = atomic_mass(&element);
        atoms.push(Atom {
            element,
            pos: Vector3::new(x, y, z),
            mass,
        });
    }
    Ok(atoms)
}
