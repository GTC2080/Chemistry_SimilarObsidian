// Reason: This file owns the chemistry stoichiometry numeric propagation rules
// formerly implemented in the Tauri Rust backend.

#include "chemistry/stoichiometry.h"

#include <cmath>

namespace kernel::chemistry {
namespace {

double to_non_negative(const double value) {
  return std::isfinite(value) && value > 0.0 ? value : 0.0;
}

std::size_t find_reference_index(
    const kernel_stoichiometry_row_input* rows,
    const std::size_t count) {
  for (std::size_t index = 0; index < count; ++index) {
    if (rows[index].is_reference != 0) {
      return index;
    }
  }
  return 0;
}

bool has_positive_density(const kernel_stoichiometry_row_input& row) {
  return row.has_density != 0 && row.density > 0.0;
}

}  // namespace

void recalculate_stoichiometry_rows(
    const kernel_stoichiometry_row_input* rows,
    const std::size_t count,
    kernel_stoichiometry_row_output* out_rows) {
  if (count == 0) {
    return;
  }

  const std::size_t reference_index = find_reference_index(rows, count);
  const double reference_moles = to_non_negative(rows[reference_index].moles);

  for (std::size_t index = 0; index < count; ++index) {
    const auto& row = rows[index];
    auto& out = out_rows[index];

    const bool is_reference = index == reference_index;
    const double eq = is_reference ? 1.0 : to_non_negative(row.eq);
    const double moles = is_reference ? reference_moles : reference_moles * eq;
    const double mw = to_non_negative(row.mw);
    const double mass = moles * mw;

    bool has_density = false;
    double density = 0.0;
    if (has_positive_density(row)) {
      has_density = true;
      density = row.density;
    } else if (row.mass > 0.0 && row.volume > 0.0) {
      has_density = true;
      density = row.mass / row.volume;
    }

    const double volume = has_density && density > 0.0 ? mass / density : 0.0;

    out.mw = mw;
    out.eq = eq;
    out.moles = moles;
    out.mass = mass;
    out.volume = volume;
    out.density = has_density ? density : 0.0;
    out.has_density = has_density ? 1 : 0;
    out.is_reference = is_reference ? 1 : 0;
  }
}

}  // namespace kernel::chemistry
