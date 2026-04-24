use serde::{Deserialize, Serialize};

const EPS: f64 = 1.0e-12;

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

#[derive(Debug, Clone, Copy)]
struct State {
    i: f64,
    m: f64,
    cta: f64,
    r: f64,
    l0: f64,
    l1: f64,
    l2: f64,
    d0: f64,
    d1: f64,
    d2: f64,
}

#[derive(Debug, Clone, Copy)]
struct Derivative {
    di: f64,
    dm: f64,
    dcta: f64,
    dr: f64,
    dl0: f64,
    dl1: f64,
    dl2: f64,
    dd0: f64,
    dd1: f64,
    dd2: f64,
}

impl State {
    fn add_scaled(self, k: Derivative, dt: f64) -> Self {
        Self {
            i: self.i + k.di * dt,
            m: self.m + k.dm * dt,
            cta: self.cta + k.dcta * dt,
            r: self.r + k.dr * dt,
            l0: self.l0 + k.dl0 * dt,
            l1: self.l1 + k.dl1 * dt,
            l2: self.l2 + k.dl2 * dt,
            d0: self.d0 + k.dd0 * dt,
            d1: self.d1 + k.dd1 * dt,
            d2: self.d2 + k.dd2 * dt,
        }
    }

    fn clamped(self) -> Self {
        Self {
            i: self.i.max(0.0),
            m: self.m.max(0.0),
            cta: self.cta.max(0.0),
            r: self.r.max(0.0),
            l0: self.l0.max(0.0),
            l1: self.l1.max(0.0),
            l2: self.l2.max(0.0),
            d0: self.d0.max(0.0),
            d1: self.d1.max(0.0),
            d2: self.d2.max(0.0),
        }
    }
}

impl Derivative {
    fn combine_rk4(k1: Self, k2: Self, k3: Self, k4: Self) -> Self {
        Self {
            di: (k1.di + 2.0 * k2.di + 2.0 * k3.di + k4.di) / 6.0,
            dm: (k1.dm + 2.0 * k2.dm + 2.0 * k3.dm + k4.dm) / 6.0,
            dcta: (k1.dcta + 2.0 * k2.dcta + 2.0 * k3.dcta + k4.dcta) / 6.0,
            dr: (k1.dr + 2.0 * k2.dr + 2.0 * k3.dr + k4.dr) / 6.0,
            dl0: (k1.dl0 + 2.0 * k2.dl0 + 2.0 * k3.dl0 + k4.dl0) / 6.0,
            dl1: (k1.dl1 + 2.0 * k2.dl1 + 2.0 * k3.dl1 + k4.dl1) / 6.0,
            dl2: (k1.dl2 + 2.0 * k2.dl2 + 2.0 * k3.dl2 + k4.dl2) / 6.0,
            dd0: (k1.dd0 + 2.0 * k2.dd0 + 2.0 * k3.dd0 + k4.dd0) / 6.0,
            dd1: (k1.dd1 + 2.0 * k2.dd1 + 2.0 * k3.dd1 + k4.dd1) / 6.0,
            dd2: (k1.dd2 + 2.0 * k2.dd2 + 2.0 * k3.dd2 + k4.dd2) / 6.0,
        }
    }
}

fn validate_params(params: &KineticsParams) -> Result<(), String> {
    if !params.m0.is_finite() || params.m0 <= 0.0 {
        return Err("m0 必须为正数".to_string());
    }
    if !params.i0.is_finite() || params.i0 < 0.0 {
        return Err("i0 不能为负数".to_string());
    }
    if !params.cta0.is_finite() || params.cta0 < 0.0 {
        return Err("cta0 不能为负数".to_string());
    }
    if !params.kd.is_finite() || params.kd < 0.0 {
        return Err("kd 不能为负数".to_string());
    }
    if !params.kp.is_finite() || params.kp < 0.0 {
        return Err("kp 不能为负数".to_string());
    }
    if !params.kt.is_finite() || params.kt < 0.0 {
        return Err("kt 不能为负数".to_string());
    }
    if !params.ktr.is_finite() || params.ktr < 0.0 {
        return Err("ktr 不能为负数".to_string());
    }
    if !params.time_max.is_finite() || params.time_max <= 0.0 {
        return Err("timeMax 必须为正数".to_string());
    }
    if params.steps < 10 || params.steps > 50_000 {
        return Err("steps 需在 10 到 50000 之间".to_string());
    }
    Ok(())
}

fn deriv(s: State, p: &KineticsParams) -> Derivative {
    let i = s.i.max(0.0);
    let m = s.m.max(0.0);
    let cta = s.cta.max(0.0);
    let r = s.r.max(0.0);
    let l0 = s.l0.max(0.0);
    let l1 = s.l1.max(0.0);
    let l2 = s.l2.max(0.0);

    // Initiator decomposition and radical generation.
    let ri = 2.0 * p.kd * i;
    let rt = p.kt * r * r;
    let rtr = p.ktr * cta * r;
    let rp = p.kp * m * r;

    // Effective deactivation for live chains.
    let k_loss = p.kt * r + p.ktr * cta;

    let di = -p.kd * i;
    let dm = -rp;
    let dcta = -rtr;
    let dr = ri - 2.0 * rt - rtr;

    // Live moments (0/1/2): initiation + propagation + loss.
    let dl0 = ri - k_loss * l0;
    let dl1 = p.kp * m * l0 - k_loss * l1;
    let dl2 = p.kp * m * (2.0 * l1 + l0) - k_loss * l2;

    // Dead moments from termination and transfer.
    let dead_rate = p.kt * r + p.ktr * cta;
    let dd0 = dead_rate * l0;
    let dd1 = dead_rate * l1;
    let dd2 = dead_rate * l2;

    Derivative {
        di,
        dm,
        dcta,
        dr,
        dl0,
        dl1,
        dl2,
        dd0,
        dd1,
        dd2,
    }
}

fn rk4_step(state: State, params: &KineticsParams, dt: f64) -> State {
    let k1 = deriv(state, params);
    let k2 = deriv(state.add_scaled(k1, dt * 0.5).clamped(), params);
    let k3 = deriv(state.add_scaled(k2, dt * 0.5).clamped(), params);
    let k4 = deriv(state.add_scaled(k3, dt).clamped(), params);
    let k = Derivative::combine_rk4(k1, k2, k3, k4);
    state.add_scaled(k, dt).clamped()
}

fn compute_mn_pdi(state: &State, monomer_factor: f64) -> (f64, f64) {
    // Prefer dead moments once chains are formed; fallback to live moments in very early region.
    let mut mu0 = state.d0;
    let mut mu1 = state.d1;
    let mut mu2 = state.d2;

    if mu0 <= EPS || mu1 <= EPS {
        mu0 = state.l0;
        mu1 = state.l1;
        mu2 = state.l2;
    }

    if mu0 <= EPS || mu1 <= EPS {
        return (0.0, 1.0);
    }

    let mn = (monomer_factor * (mu1 / mu0)).max(0.0);
    let pdi = ((mu2 * mu0) / (mu1 * mu1)).max(1.0);

    if !mn.is_finite() || !pdi.is_finite() {
        return (0.0, 1.0);
    }

    (mn, pdi)
}

pub fn simulate_polymerization(params: KineticsParams) -> Result<KineticsResult, String> {
    validate_params(&params)?;

    let n = params.steps;
    let dt = params.time_max / n as f64;

    let mut state = State {
        i: params.i0,
        m: params.m0,
        cta: params.cta0,
        r: 0.0,
        l0: 0.0,
        l1: 0.0,
        l2: 0.0,
        d0: 0.0,
        d1: 0.0,
        d2: 0.0,
    };

    let mut time = Vec::with_capacity(n + 1);
    let mut conversion = Vec::with_capacity(n + 1);
    let mut mn = Vec::with_capacity(n + 1);
    let mut pdi = Vec::with_capacity(n + 1);

    for step in 0..=n {
        let t = step as f64 * dt;
        let x = (1.0 - state.m / params.m0).clamp(0.0, 1.0);
        let (curr_mn, curr_pdi) = compute_mn_pdi(&state, params.m0);

        time.push(t);
        conversion.push(if x.is_finite() { x } else { 0.0 });
        mn.push(curr_mn);
        pdi.push(curr_pdi);

        if step < n {
            state = rk4_step(state, &params, dt);
        }
    }

    Ok(KineticsResult {
        time,
        conversion,
        mn,
        pdi,
    })
}
