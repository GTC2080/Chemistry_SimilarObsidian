use crate::models::{SpectroscopyData, SpectrumSeries};

fn parse_csv_spectroscopy(raw: &str) -> Result<SpectroscopyData, String> {
    let lines: Vec<&str> = raw
        .lines()
        .map(|l| l.trim())
        .filter(|l| !l.is_empty())
        .collect();

    let mut header_row = String::new();
    let mut data_lines: Vec<String> = Vec::new();

    for line in lines {
        if line.starts_with('#') || line.starts_with('%') {
            continue;
        }

        let delimiter = if line.contains('\t') { '\t' } else { ',' };
        let parts: Vec<&str> = line.split(delimiter).map(|s| s.trim()).collect();

        if parts.len() >= 2 && parts[0].parse::<f64>().is_err() {
            header_row = line.to_string();
            continue;
        }

        if parts.len() >= 2 {
            data_lines.push(line.to_string());
        }
    }

    if data_lines.is_empty() {
        return Err("CSV 中未找到有效的数值数据行".to_string());
    }

    let delimiter = if data_lines[0].contains('\t') {
        '\t'
    } else {
        ','
    };
    let first_parts: Vec<&str> = data_lines[0].split(delimiter).collect();
    let col_count = first_parts.len();
    if col_count < 2 {
        return Err("CSV 列数不足，至少需要 2 列".to_string());
    }

    let mut x: Vec<f64> = Vec::new();
    let mut columns: Vec<Vec<f64>> = (1..col_count).map(|_| Vec::new()).collect();

    for line in &data_lines {
        let parts: Vec<&str> = line.split(delimiter).map(|s| s.trim()).collect();
        let x_val = match parts[0].parse::<f64>() {
            Ok(v) => v,
            Err(_) => continue,
        };
        x.push(x_val);
        for c in 1..col_count {
            let val = if c < parts.len() {
                parts[c].parse::<f64>().unwrap_or(0.0)
            } else {
                0.0
            };
            columns[c - 1].push(val);
        }
    }

    if x.is_empty() {
        return Err("无法从 CSV 中提取有效数据点".to_string());
    }

    let mut x_label = "X".to_string();
    let mut y_labels: Vec<String> = Vec::new();
    if !header_row.is_empty() {
        let h_delim = if header_row.contains('\t') { '\t' } else { ',' };
        let headers: Vec<&str> = header_row.split(h_delim).map(|s| s.trim()).collect();
        if headers.len() >= 2 {
            x_label = headers[0].to_string();
            for h in headers.iter().skip(1) {
                y_labels.push((*h).to_string());
            }
        }
    }

    let series: Vec<SpectrumSeries> = columns
        .into_iter()
        .enumerate()
        .map(|(i, col)| SpectrumSeries {
            y: col,
            label: y_labels
                .get(i)
                .cloned()
                .unwrap_or_else(|| format!("Series {}", i + 1)),
        })
        .collect();

    let sample = x.iter().take(100);
    let mut x_min = f64::INFINITY;
    let mut x_max = f64::NEG_INFINITY;
    for v in sample {
        x_min = x_min.min(*v);
        x_max = x_max.max(*v);
    }
    let is_nmr = x_min >= -2.0 && x_max <= 220.0 && (x_max - x_min) < 250.0;

    Ok(SpectroscopyData {
        x,
        series,
        x_label,
        title: String::new(),
        is_nmr,
    })
}

fn parse_numeric_pairs(line: &str) -> Vec<(f64, f64)> {
    let normalized = line.replace([',', ';', '\t'], " ");
    let numbers: Vec<f64> = normalized
        .split_whitespace()
        .filter_map(|s| s.parse::<f64>().ok())
        .collect();

    let mut pairs: Vec<(f64, f64)> = Vec::new();
    let mut i = 0usize;
    while i + 1 < numbers.len() {
        pairs.push((numbers[i], numbers[i + 1]));
        i += 2;
    }
    pairs
}

fn parse_jdx_spectroscopy(raw: &str) -> Result<SpectroscopyData, String> {
    let mut title = String::new();
    let mut x_label = "X".to_string();
    let mut y_label = "Y".to_string();
    let mut data_type = String::new();
    let mut x: Vec<f64> = Vec::new();
    let mut y: Vec<f64> = Vec::new();
    let mut in_data = false;

    for line in raw.lines() {
        let trimmed = line.trim();
        if trimmed.is_empty() {
            continue;
        }

        if let Some(v) = trimmed.strip_prefix("##TITLE=") {
            title = v.trim().to_string();
            continue;
        }
        if let Some(v) = trimmed.strip_prefix("##XUNITS=") {
            x_label = v.trim().to_string();
            continue;
        }
        if let Some(v) = trimmed.strip_prefix("##YUNITS=") {
            y_label = v.trim().to_string();
            continue;
        }
        if let Some(v) = trimmed.strip_prefix("##DATATYPE=") {
            data_type = v.trim().to_string();
            continue;
        }
        if trimmed.starts_with("##XYDATA=") || trimmed.starts_with("##PEAK TABLE=") {
            in_data = true;
            continue;
        }
        if trimmed.starts_with("##END=") {
            break;
        }
        if trimmed.starts_with("##") {
            in_data = false;
            continue;
        }

        if in_data {
            let pairs = parse_numeric_pairs(trimmed);
            for (xv, yv) in pairs {
                x.push(xv);
                y.push(yv);
            }
        }
    }

    if x.is_empty() {
        return Err("JDX 文件中未找到可解析的数据点".to_string());
    }

    let data_type_lower = data_type.to_lowercase();
    let x_units_lower = x_label.to_lowercase();
    let is_nmr = data_type_lower.contains("nmr")
        || x_units_lower.contains("ppm")
        || x_units_lower.contains("chemical shift");

    Ok(SpectroscopyData {
        x,
        series: vec![SpectrumSeries { y, label: y_label }],
        x_label,
        title,
        is_nmr,
    })
}

pub fn parse_spectroscopy_from_text(
    raw: &str,
    extension: &str,
) -> Result<SpectroscopyData, String> {
    if extension.eq_ignore_ascii_case("jdx") {
        return parse_jdx_spectroscopy(raw);
    }
    if extension.eq_ignore_ascii_case("csv") {
        return parse_csv_spectroscopy(raw);
    }
    Err(format!("不支持的波谱文件扩展名: {}", extension))
}
