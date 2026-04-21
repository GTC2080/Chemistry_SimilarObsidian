// Reason: Keep runtime recovery disk-truth suite composition thin so catch-up and replacement scenarios can evolve independently.

#include "api/kernel_api_runtime_recovery_disk_truth_suites.h"
#include "api/kernel_api_runtime_recovery_suites.h"

void run_runtime_recovery_disk_truth_tests() {
  run_runtime_recovery_disk_truth_catchup_tests();
  run_runtime_recovery_disk_truth_replace_tests();
}
