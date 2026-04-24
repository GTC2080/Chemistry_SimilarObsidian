// Reason: This file exposes the polymerization kinetics compute surface so
// Tauri Rust can bridge chemistry calculation without owning the algorithm.

#include "kernel/c_api.h"

#include "chemistry/polymerization_kinetics.h"
#include "core/kernel_shared.h"

#include <algorithm>
#include <new>

namespace {

void reset_polymerization_kinetics_result_impl(
    kernel_polymerization_kinetics_result* result) {
  if (result == nullptr) {
    return;
  }

  delete[] result->time;
  delete[] result->conversion;
  delete[] result->mn;
  delete[] result->pdi;
  result->time = nullptr;
  result->conversion = nullptr;
  result->mn = nullptr;
  result->pdi = nullptr;
  result->count = 0;
}

bool allocate_result_arrays(
    const std::size_t count,
    kernel_polymerization_kinetics_result* out_result) {
  out_result->time = new (std::nothrow) double[count];
  out_result->conversion = new (std::nothrow) double[count];
  out_result->mn = new (std::nothrow) double[count];
  out_result->pdi = new (std::nothrow) double[count];
  if (out_result->time == nullptr || out_result->conversion == nullptr ||
      out_result->mn == nullptr || out_result->pdi == nullptr) {
    reset_polymerization_kinetics_result_impl(out_result);
    return false;
  }
  out_result->count = count;
  return true;
}

}  // namespace

extern "C" kernel_status kernel_simulate_polymerization_kinetics(
    const kernel_polymerization_kinetics_params* params,
    kernel_polymerization_kinetics_result* out_result) {
  reset_polymerization_kinetics_result_impl(out_result);
  if (params == nullptr || out_result == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel::chemistry::PolymerizationKineticsSeries series;
  if (!kernel::chemistry::simulate_polymerization_kinetics(*params, series)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::size_t count = series.time.size();
  if (count == 0 || series.conversion.size() != count ||
      series.mn.size() != count || series.pdi.size() != count) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  if (!allocate_result_arrays(count, out_result)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }

  std::copy(series.time.begin(), series.time.end(), out_result->time);
  std::copy(series.conversion.begin(), series.conversion.end(), out_result->conversion);
  std::copy(series.mn.begin(), series.mn.end(), out_result->mn);
  std::copy(series.pdi.begin(), series.pdi.end(), out_result->pdi);
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" void kernel_free_polymerization_kinetics_result(
    kernel_polymerization_kinetics_result* result) {
  reset_polymerization_kinetics_result_impl(result);
}
