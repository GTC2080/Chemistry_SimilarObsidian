use serde::{Deserialize, Serialize};

const KERNEL_OK: i32 = 0;

#[repr(C)]
#[derive(Debug, Clone, Copy)]
struct KernelStatus {
    code: i32,
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
struct KernelPolymerizationKineticsParams {
    m0: f64,
    i0: f64,
    cta0: f64,
    kd: f64,
    kp: f64,
    kt: f64,
    ktr: f64,
    time_max: f64,
    steps: usize,
}

#[repr(C)]
#[derive(Debug)]
struct KernelPolymerizationKineticsResult {
    time: *mut f64,
    conversion: *mut f64,
    mn: *mut f64,
    pdi: *mut f64,
    count: usize,
}

impl Default for KernelPolymerizationKineticsResult {
    fn default() -> Self {
        Self {
            time: std::ptr::null_mut(),
            conversion: std::ptr::null_mut(),
            mn: std::ptr::null_mut(),
            pdi: std::ptr::null_mut(),
            count: 0,
        }
    }
}

extern "C" {
    fn kernel_simulate_polymerization_kinetics(
        params: *const KernelPolymerizationKineticsParams,
        out_result: *mut KernelPolymerizationKineticsResult,
    ) -> KernelStatus;
    fn kernel_free_polymerization_kinetics_result(result: *mut KernelPolymerizationKineticsResult);
}

#[derive(Debug, Clone, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct KineticsParams {
    pub m0: f64,
    pub i0: f64,
    pub cta0: f64,
    pub kd: f64,
    pub kp: f64,
    pub kt: f64,
    pub ktr: f64,
    pub time_max: f64,
    pub steps: usize,
}

#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct KineticsResult {
    pub time: Vec<f64>,
    pub conversion: Vec<f64>,
    pub mn: Vec<f64>,
    pub pdi: Vec<f64>,
}

fn kernel_params_from(params: &KineticsParams) -> KernelPolymerizationKineticsParams {
    KernelPolymerizationKineticsParams {
        m0: params.m0,
        i0: params.i0,
        cta0: params.cta0,
        kd: params.kd,
        kp: params.kp,
        kt: params.kt,
        ktr: params.ktr,
        time_max: params.time_max,
        steps: params.steps,
    }
}

fn status_message(status: KernelStatus) -> String {
    match status.code {
        1 => "聚合动力学参数无效".to_string(),
        5 => "聚合动力学内核计算失败".to_string(),
        code => format!("聚合动力学内核返回错误状态: {code}"),
    }
}

unsafe fn copy_series(ptr: *const f64, count: usize, label: &str) -> Result<Vec<f64>, String> {
    if count == 0 {
        return Ok(Vec::new());
    }
    if ptr.is_null() {
        return Err(format!("聚合动力学内核缺少 {label} 输出"));
    }
    Ok(std::slice::from_raw_parts(ptr, count).to_vec())
}

unsafe fn result_from_raw(
    raw: &KernelPolymerizationKineticsResult,
) -> Result<KineticsResult, String> {
    let count = raw.count;
    if count == 0 {
        return Err("聚合动力学内核返回空结果".to_string());
    }

    Ok(KineticsResult {
        time: copy_series(raw.time, count, "time")?,
        conversion: copy_series(raw.conversion, count, "conversion")?,
        mn: copy_series(raw.mn, count, "mn")?,
        pdi: copy_series(raw.pdi, count, "pdi")?,
    })
}

pub fn simulate_polymerization(params: KineticsParams) -> Result<KineticsResult, String> {
    let kernel_params = kernel_params_from(&params);
    let mut raw = KernelPolymerizationKineticsResult::default();
    let status = unsafe { kernel_simulate_polymerization_kinetics(&kernel_params, &mut raw) };
    if status.code != KERNEL_OK {
        unsafe { kernel_free_polymerization_kinetics_result(&mut raw) };
        return Err(status_message(status));
    }

    let result = unsafe { result_from_raw(&raw) };
    unsafe { kernel_free_polymerization_kinetics_result(&mut raw) };
    result
}
