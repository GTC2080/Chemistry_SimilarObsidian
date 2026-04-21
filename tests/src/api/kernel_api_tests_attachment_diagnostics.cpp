// Reason: Keep attachment diagnostics entry wiring thin so snapshot and recount coverage stay separated.

#include "api/kernel_api_attachment_diagnostics_suites.h"

void run_attachment_diagnostics_tests() {
  run_attachment_diagnostics_snapshot_tests();
  run_attachment_diagnostics_recount_tests();
}
