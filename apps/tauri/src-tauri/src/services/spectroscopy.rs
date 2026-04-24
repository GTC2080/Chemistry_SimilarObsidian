use std::ffi::{CStr, CString};
use std::os::raw::c_char;

use crate::models::{SpectroscopyData, SpectrumSeries};

const KERNEL_OK: i32 = 0;
const KERNEL_SPECTROSCOPY_PARSE_ERROR_UNSUPPORTED_EXTENSION: i32 = 1;
const KERNEL_SPECTROSCOPY_PARSE_ERROR_CSV_NO_NUMERIC_ROWS: i32 = 2;
const KERNEL_SPECTROSCOPY_PARSE_ERROR_CSV_TOO_FEW_COLUMNS: i32 = 3;
const KERNEL_SPECTROSCOPY_PARSE_ERROR_CSV_NO_VALID_POINTS: i32 = 4;
const KERNEL_SPECTROSCOPY_PARSE_ERROR_JDX_NO_POINTS: i32 = 5;

#[repr(C)]
#[derive(Debug, Clone, Copy)]
struct KernelStatus {
    code: i32,
}

#[repr(C)]
#[derive(Debug)]
struct KernelSpectrumSeries {
    y: *mut f64,
    count: usize,
    label: *mut c_char,
}

#[repr(C)]
#[derive(Debug)]
struct KernelSpectroscopyData {
    x: *mut f64,
    x_count: usize,
    series: *mut KernelSpectrumSeries,
    series_count: usize,
    x_label: *mut c_char,
    title: *mut c_char,
    is_nmr: u8,
    error: i32,
}

impl Default for KernelSpectroscopyData {
    fn default() -> Self {
        Self {
            x: std::ptr::null_mut(),
            x_count: 0,
            series: std::ptr::null_mut(),
            series_count: 0,
            x_label: std::ptr::null_mut(),
            title: std::ptr::null_mut(),
            is_nmr: 0,
            error: 0,
        }
    }
}

extern "C" {
    fn kernel_parse_spectroscopy_text(
        raw: *const c_char,
        raw_size: usize,
        extension: *const c_char,
        out_data: *mut KernelSpectroscopyData,
    ) -> KernelStatus;
    fn kernel_free_spectroscopy_data(data: *mut KernelSpectroscopyData);
}

fn spectroscopy_error_message(error: i32, extension: &str) -> String {
    match error {
        KERNEL_SPECTROSCOPY_PARSE_ERROR_UNSUPPORTED_EXTENSION => {
            format!("不支持的波谱文件扩展名: {}", extension)
        }
        KERNEL_SPECTROSCOPY_PARSE_ERROR_CSV_NO_NUMERIC_ROWS => {
            "CSV 中未找到有效的数值数据行".to_string()
        }
        KERNEL_SPECTROSCOPY_PARSE_ERROR_CSV_TOO_FEW_COLUMNS => {
            "CSV 列数不足，至少需要 2 列".to_string()
        }
        KERNEL_SPECTROSCOPY_PARSE_ERROR_CSV_NO_VALID_POINTS => {
            "无法从 CSV 中提取有效数据点".to_string()
        }
        KERNEL_SPECTROSCOPY_PARSE_ERROR_JDX_NO_POINTS => {
            "JDX 文件中未找到可解析的数据点".to_string()
        }
        _ => "波谱内核解析失败".to_string(),
    }
}

unsafe fn copy_f64_values(ptr: *const f64, count: usize, label: &str) -> Result<Vec<f64>, String> {
    if count == 0 {
        return Ok(Vec::new());
    }
    if ptr.is_null() {
        return Err(format!("波谱内核缺少 {label} 输出"));
    }
    Ok(std::slice::from_raw_parts(ptr, count).to_vec())
}

unsafe fn string_from_kernel(ptr: *const c_char, label: &str) -> Result<String, String> {
    if ptr.is_null() {
        return Err(format!("波谱内核缺少 {label} 输出"));
    }
    Ok(CStr::from_ptr(ptr).to_string_lossy().into_owned())
}

unsafe fn spectroscopy_from_kernel(
    raw: &KernelSpectroscopyData,
) -> Result<SpectroscopyData, String> {
    if raw.x_count == 0 || raw.series_count == 0 {
        return Err("波谱内核返回空结果".to_string());
    }
    if raw.series.is_null() {
        return Err("波谱内核缺少 series 输出".to_string());
    }

    let x = copy_f64_values(raw.x, raw.x_count, "x")?;
    let raw_series = std::slice::from_raw_parts(raw.series, raw.series_count);
    let mut series = Vec::with_capacity(raw_series.len());
    for (index, item) in raw_series.iter().enumerate() {
        series.push(SpectrumSeries {
            y: copy_f64_values(item.y, item.count, &format!("series[{index}]"))?,
            label: string_from_kernel(item.label, &format!("series[{index}].label"))?,
        });
    }

    Ok(SpectroscopyData {
        x,
        series,
        x_label: string_from_kernel(raw.x_label, "x_label")?,
        title: string_from_kernel(raw.title, "title")?,
        is_nmr: raw.is_nmr != 0,
    })
}

pub fn parse_spectroscopy_from_text(
    raw: &str,
    extension: &str,
) -> Result<SpectroscopyData, String> {
    let extension_c =
        CString::new(extension).map_err(|_| format!("不支持的波谱文件扩展名: {}", extension))?;
    let mut kernel_data = KernelSpectroscopyData::default();
    let status = unsafe {
        kernel_parse_spectroscopy_text(
            raw.as_ptr().cast::<c_char>(),
            raw.len(),
            extension_c.as_ptr(),
            &mut kernel_data,
        )
    };

    if status.code != KERNEL_OK {
        let message = spectroscopy_error_message(kernel_data.error, extension);
        unsafe { kernel_free_spectroscopy_data(&mut kernel_data) };
        return Err(message);
    }

    let result = unsafe { spectroscopy_from_kernel(&kernel_data) };
    unsafe { kernel_free_spectroscopy_data(&mut kernel_data) };
    result
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parse_spectroscopy_from_text_uses_kernel_csv_parser() {
        let parsed = parse_spectroscopy_from_text(
            "ppm,intensity,fit\n1.0,5.0,4.5\n2.0,6.0,bad\n3.0,7.0\n",
            "csv",
        )
        .expect("kernel csv parse");

        assert_eq!(parsed.x, vec![1.0, 2.0, 3.0]);
        assert_eq!(parsed.x_label, "ppm");
        assert!(parsed.is_nmr);
        assert_eq!(parsed.series.len(), 2);
        assert_eq!(parsed.series[0].label, "intensity");
        assert_eq!(parsed.series[0].y, vec![5.0, 6.0, 7.0]);
        assert_eq!(parsed.series[1].label, "fit");
        assert_eq!(parsed.series[1].y, vec![4.5, 0.0, 0.0]);
    }

    #[test]
    fn parse_spectroscopy_from_text_uses_kernel_jdx_parser() {
        let parsed = parse_spectroscopy_from_text(
            "##TITLE=Sample NMR\n\
             ##DATATYPE=NMR SPECTRUM\n\
             ##XUNITS=PPM\n\
             ##YUNITS=INTENSITY\n\
             ##PEAK TABLE=(XY..XY)\n\
             1.0, 10.0; 2.0, 11.0\n\
             ##END=\n",
            "jdx",
        )
        .expect("kernel jdx parse");

        assert_eq!(parsed.title, "Sample NMR");
        assert_eq!(parsed.x_label, "PPM");
        assert!(parsed.is_nmr);
        assert_eq!(parsed.x, vec![1.0, 2.0]);
        assert_eq!(parsed.series.len(), 1);
        assert_eq!(parsed.series[0].label, "INTENSITY");
        assert_eq!(parsed.series[0].y, vec![10.0, 11.0]);
    }

    #[test]
    fn parse_spectroscopy_from_text_maps_kernel_parse_errors() {
        let err = parse_spectroscopy_from_text("name,value\nnot-a-number,still-bad\n", "csv")
            .expect_err("invalid csv");
        assert_eq!(err, "CSV 中未找到有效的数值数据行");

        let err = parse_spectroscopy_from_text("1,2\n", "txt").expect_err("unsupported");
        assert_eq!(err, "不支持的波谱文件扩展名: txt");
    }
}
