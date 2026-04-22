// Reason: This file keeps the background rebuild suite composition thin while lifecycle and wait semantics live in smaller topic files.

#include "api/kernel_api_rebuild_runtime_background_suites.h"
#include "api/kernel_api_rebuild_runtime_suites.h"

void run_rebuild_runtime_background_tests() {
  run_rebuild_runtime_background_lifecycle_tests();
  run_rebuild_runtime_background_wait_tests();
}
