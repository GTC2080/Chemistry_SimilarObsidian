// Reason: This file keeps the watcher smoke suite composition thin while the concrete regressions live in topic-specific files.

#include "api/kernel_api_test_suites.h"
#include "api/kernel_api_watcher_smoke_suites.h"

void run_kernel_api_watcher_smoke_tests() {
  run_kernel_api_watcher_basic_smoke_tests();
  run_kernel_api_watcher_close_edge_tests();
}
