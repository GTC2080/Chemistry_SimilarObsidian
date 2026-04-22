// Reason: Keep startup recovery suite composition thin so cleanup and journal-tail scenarios can evolve independently.

#include "api/kernel_api_runtime_recovery_startup_suites.h"
#include "api/kernel_api_runtime_recovery_suites.h"

void run_runtime_recovery_startup_tests() {
  run_runtime_recovery_startup_cleanup_tests();
  run_runtime_recovery_startup_tail_tests();
}
