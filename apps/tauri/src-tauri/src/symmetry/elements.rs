pub(crate) fn normalize_element(raw: &str) -> String {
    let s: String = raw.chars().filter(|c| c.is_alphabetic()).collect();
    if s.is_empty() {
        return "X".into();
    }
    let mut chars = s.chars();
    let first = chars.next().unwrap().to_uppercase().to_string();
    let rest: String = chars
        .take(1)
        .map(|c| c.to_lowercase().next().unwrap())
        .collect();
    format!("{}{}", first, rest)
}

pub(super) fn atomic_mass(element: &str) -> f64 {
    match element {
        "H" => 1.008,
        "He" => 4.003,
        "Li" => 6.941,
        "Be" => 9.012,
        "B" => 10.81,
        "C" => 12.011,
        "N" => 14.007,
        "O" => 15.999,
        "F" => 18.998,
        "Ne" => 20.180,
        "Na" => 22.990,
        "Mg" => 24.305,
        "Al" => 26.982,
        "Si" => 28.086,
        "P" => 30.974,
        "S" => 32.065,
        "Cl" => 35.453,
        "Ar" => 39.948,
        "K" => 39.098,
        "Ca" => 40.078,
        "Ti" => 47.867,
        "V" => 50.942,
        "Cr" => 51.996,
        "Mn" => 54.938,
        "Fe" => 55.845,
        "Co" => 58.933,
        "Ni" => 58.693,
        "Cu" => 63.546,
        "Zn" => 65.38,
        "Ga" => 69.723,
        "Ge" => 72.63,
        "As" => 74.922,
        "Se" => 78.971,
        "Br" => 79.904,
        "Kr" => 83.798,
        "Rb" => 85.468,
        "Sr" => 87.62,
        "Zr" => 91.224,
        "Mo" => 95.95,
        "Ru" => 101.07,
        "Rh" => 102.91,
        "Pd" => 106.42,
        "Ag" => 107.87,
        "Cd" => 112.41,
        "In" => 114.82,
        "Sn" => 118.71,
        "Sb" => 121.76,
        "Te" => 127.60,
        "I" => 126.90,
        "Xe" => 131.29,
        "Cs" => 132.91,
        "Ba" => 137.33,
        "La" => 138.91,
        "Pt" => 195.08,
        "Au" => 196.97,
        "Hg" => 200.59,
        "Pb" => 207.2,
        "Bi" => 208.98,
        "U" => 238.03,
        _ => 12.0, // 未知元素默认碳质量
    }
}
