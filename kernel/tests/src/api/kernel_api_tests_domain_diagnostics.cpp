// Reason: Keep Track 4 domain-diagnostics entry wiring thin so snapshot and
// recount coverage stay separated.

#include "api/kernel_api_domain_diagnostics_suites.h"

void run_domain_diagnostics_tests() {
  run_domain_diagnostics_snapshot_tests();
  run_domain_diagnostics_recount_tests();
}
