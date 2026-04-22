// Reason: Keep rebuild runtime entry wiring thin so core, background-task, and status regressions stay separated.

#include "api/kernel_api_rebuild_runtime_suites.h"

void run_rebuild_runtime_tests() {
  run_rebuild_runtime_core_tests();
  run_rebuild_runtime_background_tests();
  run_rebuild_runtime_status_tests();
}
