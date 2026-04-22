// Reason: Keep runtime recovery entry wiring thin so startup recovery and reopen repair suites can evolve independently.

#include "api/kernel_api_runtime_recovery_interruptions.h"
#include "api/kernel_api_runtime_recovery_suites.h"

void run_runtime_recovery_tests() {
  run_runtime_recovery_startup_tests();
  run_runtime_recovery_disk_truth_tests();
  run_runtime_recovery_backoff_tests();
  run_runtime_recovery_interrupted_apply_tests();
}
