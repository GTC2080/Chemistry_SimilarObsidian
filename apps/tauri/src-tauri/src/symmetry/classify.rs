use super::types::{FoundAxis, FoundPlane};

pub(super) fn classify_point_group(
    axes: &[FoundAxis],
    planes: &[FoundPlane],
    has_inversion: bool,
) -> String {
    let n_planes = planes.len();
    let highest_order = axes.first().map(|a| a.order).unwrap_or(1);

    let count_axes = |order: u8| -> usize { axes.iter().filter(|a| a.order == order).count() };

    let n_c3 = count_axes(3);
    let n_c4 = count_axes(4);
    let n_c5 = count_axes(5);

    if n_c5 >= 6 {
        return if has_inversion { "I_h" } else { "I" }.into();
    }

    if n_c4 >= 3 {
        return if has_inversion { "O_h" } else { "O" }.into();
    }

    if n_c3 >= 4 && highest_order <= 3 {
        if n_planes >= 6 {
            return "T_d".into();
        }
        return if has_inversion { "T_h" } else { "T" }.into();
    }

    if highest_order == 1 {
        if n_planes > 0 {
            return "C_s".into();
        }
        if has_inversion {
            return "C_i".into();
        }
        return "C_1".into();
    }

    let n = highest_order;
    let principal_dir = axes[0].dir;

    let perp_c2_count = axes
        .iter()
        .filter(|a| a.order == 2 && a.dir.dot(&principal_dir).abs() < 0.3)
        .count();

    let has_sigma_h = planes
        .iter()
        .any(|p| p.normal.dot(&principal_dir).abs() > 0.7);

    let sigma_v_count = planes
        .iter()
        .filter(|p| p.normal.dot(&principal_dir).abs() < 0.3)
        .count();

    if perp_c2_count >= n as usize {
        if has_sigma_h {
            return format!("D_{}h", n);
        }
        if sigma_v_count >= n as usize {
            return format!("D_{}d", n);
        }
        return format!("D_{}", n);
    }

    if has_sigma_h {
        return format!("C_{}h", n);
    }
    if sigma_v_count > 0 {
        return format!("C_{}v", n);
    }

    format!("C_{}", n)
}
