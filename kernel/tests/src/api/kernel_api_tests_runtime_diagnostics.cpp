// Reason: Keep the runtime diagnostics entrypoint thin so watcher and non-watcher diagnostics stay separated.

#include "api/kernel_api_runtime_diagnostics_suites.h"

void run_runtime_diagnostics_tests() {
  run_runtime_diagnostics_core_tests();
  run_runtime_diagnostics_watcher_tests();
}
