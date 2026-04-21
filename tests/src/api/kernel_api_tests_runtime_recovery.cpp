// Reason: Keep runtime recovery entry wiring thin so startup recovery and reopen repair suites can evolve independently.

#include "api/kernel_api_runtime_recovery_interruptions.h"
#include "api/kernel_api_runtime_recovery_suites.h"

void run_runtime_recovery_tests() {
  run_runtime_recovery_startup_tests();
  run_runtime_recovery_disk_truth_tests();
  test_close_during_watcher_fault_backoff_leaves_delete_for_reopen_catch_up();
  test_close_during_watcher_fault_backoff_leaves_modify_for_reopen_catch_up();
  test_close_during_watcher_fault_backoff_leaves_create_for_reopen_catch_up();
  test_reopen_catch_up_repairs_partial_state_left_by_interrupted_background_rebuild();
  test_reopen_catch_up_repairs_partial_state_left_by_interrupted_watcher_apply();
}
