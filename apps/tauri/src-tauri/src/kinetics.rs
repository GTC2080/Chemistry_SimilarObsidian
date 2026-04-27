use serde::{Deserialize, Serialize};

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

#[derive(Debug, Clone, Deserialize, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct KineticsResult {
    pub time: Vec<f64>,
    pub conversion: Vec<f64>,
    pub mn: Vec<f64>,
    pub pdi: Vec<f64>,
}

pub fn simulate_polymerization(params: KineticsParams) -> Result<KineticsResult, String> {
    crate::sealed_kernel::simulate_polymerization_kinetics(params).map_err(|err| err.to_string())
}

#[cfg(test)]
mod tests {
    use super::*;

    fn default_params() -> KineticsParams {
        KineticsParams {
            m0: 1.0,
            i0: 0.01,
            cta0: 0.001,
            kd: 0.001,
            kp: 100.0,
            kt: 1000.0,
            ktr: 0.1,
            time_max: 3600.0,
            steps: 120,
        }
    }

    #[test]
    fn simulate_polymerization_returns_kernel_series() {
        let params = default_params();
        let result = simulate_polymerization(params.clone()).expect("kernel kinetics result");

        assert_eq!(result.time.len(), params.steps + 1);
        assert_eq!(result.conversion.len(), result.time.len());
        assert_eq!(result.mn.len(), result.time.len());
        assert_eq!(result.pdi.len(), result.time.len());
        assert_eq!(result.time[0], 0.0);
        assert!((result.time[result.time.len() - 1] - params.time_max).abs() < 1.0e-9);
        assert!(result.conversion[result.conversion.len() - 1] > 0.0);
        assert!(result
            .pdi
            .iter()
            .all(|value| value.is_finite() && *value >= 1.0));
    }

    #[test]
    fn simulate_polymerization_rejects_invalid_params_from_kernel() {
        let mut params = default_params();
        params.m0 = 0.0;

        let err = simulate_polymerization(params).expect_err("invalid kinetics params");

        assert_eq!(err, "聚合动力学参数无效");
    }
}
