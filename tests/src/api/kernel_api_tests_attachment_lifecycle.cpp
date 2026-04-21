#include "api/kernel_api_attachment_lifecycle_suites.h"

void run_attachment_lifecycle_tests() {
  run_attachment_lifecycle_recovery_tests();
  run_attachment_lifecycle_watcher_tests();
  run_attachment_lifecycle_surface_tests();
}
