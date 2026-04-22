// Reason: Keep chemistry diagnostics entry wiring thin so Track 5 Batch 4
// snapshot and recount coverage stay split and easy to trim later.

#include "api/kernel_api_chemistry_diagnostics_suites.h"

void run_chemistry_diagnostics_tests() {
  run_chemistry_diagnostics_snapshot_tests();
  run_chemistry_diagnostics_recount_tests();
}
