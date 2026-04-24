// Reason: This file keeps the polymerization kinetics compute model in the
// kernel chemistry layer while hosts only marshal command inputs and outputs.

#pragma once

#include "kernel/types.h"

#include <vector>

namespace kernel::chemistry {

struct PolymerizationKineticsSeries {
  std::vector<double> time;
  std::vector<double> conversion;
  std::vector<double> mn;
  std::vector<double> pdi;
};

bool simulate_polymerization_kinetics(
    const kernel_polymerization_kinetics_params& params,
    PolymerizationKineticsSeries& out_series);

}  // namespace kernel::chemistry
