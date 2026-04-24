use super::types::Atom;

mod cif;
mod pdb;
mod xyz;

pub(super) fn parse_atoms(raw: &str, format: &str) -> Result<Vec<Atom>, String> {
    match format.to_lowercase().as_str() {
        "pdb" => pdb::parse_pdb(raw),
        "xyz" => xyz::parse_xyz(raw),
        "cif" => cif::parse_cif_simple(raw),
        _ => Err(format!("不支持的分子文件格式: {}", format)),
    }
}
