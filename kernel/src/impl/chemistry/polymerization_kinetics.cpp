// Reason: This file owns the polymerization RK4 compute kernel formerly hosted
// by the Tauri Rust backend.

#include "chemistry/polymerization_kinetics.h"

#include <algorithm>
#include <cmath>

namespace kernel::chemistry {
namespace {

constexpr double kEpsilon = 1.0e-12;

struct State {
  double i = 0.0;
  double m = 0.0;
  double cta = 0.0;
  double r = 0.0;
  double l0 = 0.0;
  double l1 = 0.0;
  double l2 = 0.0;
  double d0 = 0.0;
  double d1 = 0.0;
  double d2 = 0.0;
};

struct Derivative {
  double di = 0.0;
  double dm = 0.0;
  double dcta = 0.0;
  double dr = 0.0;
  double dl0 = 0.0;
  double dl1 = 0.0;
  double dl2 = 0.0;
  double dd0 = 0.0;
  double dd1 = 0.0;
  double dd2 = 0.0;
};

bool is_valid_params(const kernel_polymerization_kinetics_params& params) {
  return std::isfinite(params.m0) && params.m0 > 0.0 &&
         std::isfinite(params.i0) && params.i0 >= 0.0 &&
         std::isfinite(params.cta0) && params.cta0 >= 0.0 &&
         std::isfinite(params.kd) && params.kd >= 0.0 &&
         std::isfinite(params.kp) && params.kp >= 0.0 &&
         std::isfinite(params.kt) && params.kt >= 0.0 &&
         std::isfinite(params.ktr) && params.ktr >= 0.0 &&
         std::isfinite(params.time_max) && params.time_max > 0.0 &&
         params.steps >= 10 && params.steps <= 50000;
}

State add_scaled(const State& state, const Derivative& k, const double dt) {
  return State{
      state.i + k.di * dt,
      state.m + k.dm * dt,
      state.cta + k.dcta * dt,
      state.r + k.dr * dt,
      state.l0 + k.dl0 * dt,
      state.l1 + k.dl1 * dt,
      state.l2 + k.dl2 * dt,
      state.d0 + k.dd0 * dt,
      state.d1 + k.dd1 * dt,
      state.d2 + k.dd2 * dt};
}

State clamped(const State& state) {
  return State{
      std::max(state.i, 0.0),
      std::max(state.m, 0.0),
      std::max(state.cta, 0.0),
      std::max(state.r, 0.0),
      std::max(state.l0, 0.0),
      std::max(state.l1, 0.0),
      std::max(state.l2, 0.0),
      std::max(state.d0, 0.0),
      std::max(state.d1, 0.0),
      std::max(state.d2, 0.0)};
}

Derivative combine_rk4(
    const Derivative& k1,
    const Derivative& k2,
    const Derivative& k3,
    const Derivative& k4) {
  return Derivative{
      (k1.di + 2.0 * k2.di + 2.0 * k3.di + k4.di) / 6.0,
      (k1.dm + 2.0 * k2.dm + 2.0 * k3.dm + k4.dm) / 6.0,
      (k1.dcta + 2.0 * k2.dcta + 2.0 * k3.dcta + k4.dcta) / 6.0,
      (k1.dr + 2.0 * k2.dr + 2.0 * k3.dr + k4.dr) / 6.0,
      (k1.dl0 + 2.0 * k2.dl0 + 2.0 * k3.dl0 + k4.dl0) / 6.0,
      (k1.dl1 + 2.0 * k2.dl1 + 2.0 * k3.dl1 + k4.dl1) / 6.0,
      (k1.dl2 + 2.0 * k2.dl2 + 2.0 * k3.dl2 + k4.dl2) / 6.0,
      (k1.dd0 + 2.0 * k2.dd0 + 2.0 * k3.dd0 + k4.dd0) / 6.0,
      (k1.dd1 + 2.0 * k2.dd1 + 2.0 * k3.dd1 + k4.dd1) / 6.0,
      (k1.dd2 + 2.0 * k2.dd2 + 2.0 * k3.dd2 + k4.dd2) / 6.0};
}

Derivative derivative(const State& state, const kernel_polymerization_kinetics_params& params) {
  const double i = std::max(state.i, 0.0);
  const double m = std::max(state.m, 0.0);
  const double cta = std::max(state.cta, 0.0);
  const double r = std::max(state.r, 0.0);
  const double l0 = std::max(state.l0, 0.0);
  const double l1 = std::max(state.l1, 0.0);
  const double l2 = std::max(state.l2, 0.0);

  const double ri = 2.0 * params.kd * i;
  const double rt = params.kt * r * r;
  const double rtr = params.ktr * cta * r;
  const double rp = params.kp * m * r;
  const double k_loss = params.kt * r + params.ktr * cta;

  const double dead_rate = params.kt * r + params.ktr * cta;

  return Derivative{
      -params.kd * i,
      -rp,
      -rtr,
      ri - 2.0 * rt - rtr,
      ri - k_loss * l0,
      params.kp * m * l0 - k_loss * l1,
      params.kp * m * (2.0 * l1 + l0) - k_loss * l2,
      dead_rate * l0,
      dead_rate * l1,
      dead_rate * l2};
}

State rk4_step(
    const State& state,
    const kernel_polymerization_kinetics_params& params,
    const double dt) {
  const Derivative k1 = derivative(state, params);
  const Derivative k2 = derivative(clamped(add_scaled(state, k1, dt * 0.5)), params);
  const Derivative k3 = derivative(clamped(add_scaled(state, k2, dt * 0.5)), params);
  const Derivative k4 = derivative(clamped(add_scaled(state, k3, dt)), params);
  return clamped(add_scaled(state, combine_rk4(k1, k2, k3, k4), dt));
}

std::pair<double, double> compute_mn_pdi(const State& state, const double monomer_factor) {
  double mu0 = state.d0;
  double mu1 = state.d1;
  double mu2 = state.d2;

  if (mu0 <= kEpsilon || mu1 <= kEpsilon) {
    mu0 = state.l0;
    mu1 = state.l1;
    mu2 = state.l2;
  }

  if (mu0 <= kEpsilon || mu1 <= kEpsilon) {
    return {0.0, 1.0};
  }

  const double mn = std::max(monomer_factor * (mu1 / mu0), 0.0);
  const double pdi = std::max((mu2 * mu0) / (mu1 * mu1), 1.0);
  if (!std::isfinite(mn) || !std::isfinite(pdi)) {
    return {0.0, 1.0};
  }
  return {mn, pdi};
}

}  // namespace

bool simulate_polymerization_kinetics(
    const kernel_polymerization_kinetics_params& params,
    PolymerizationKineticsSeries& out_series) {
  out_series = PolymerizationKineticsSeries{};
  if (!is_valid_params(params)) {
    return false;
  }

  const std::size_t n = params.steps;
  const double dt = params.time_max / static_cast<double>(n);
  out_series.time.reserve(n + 1);
  out_series.conversion.reserve(n + 1);
  out_series.mn.reserve(n + 1);
  out_series.pdi.reserve(n + 1);

  State state{};
  state.i = params.i0;
  state.m = params.m0;
  state.cta = params.cta0;

  for (std::size_t step = 0; step <= n; ++step) {
    const double t = static_cast<double>(step) * dt;
    double x = 1.0 - state.m / params.m0;
    x = std::clamp(x, 0.0, 1.0);
    const auto [mn, pdi] = compute_mn_pdi(state, params.m0);

    out_series.time.push_back(t);
    out_series.conversion.push_back(std::isfinite(x) ? x : 0.0);
    out_series.mn.push_back(mn);
    out_series.pdi.push_back(pdi);

    if (step < n) {
      state = rk4_step(state, params, dt);
    }
  }

  return true;
}

}  // namespace kernel::chemistry
