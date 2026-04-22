// Reason: This file keeps the runtime diagnostics core suite composition thin while concrete diagnostics regressions live in topic-specific files.

#include "api/kernel_api_runtime_diagnostics_core_suites.h"
#include "api/kernel_api_runtime_diagnostics_suites.h"

void run_runtime_diagnostics_core_tests() {
  run_runtime_diagnostics_event_recovery_tests();
  run_runtime_diagnostics_state_tests();
  run_runtime_diagnostics_rebuild_tests();
}
