// Reason: This file keeps attachment lifecycle recovery suite composition thin while startup and reopen scenarios live in smaller topic files.

#include "api/kernel_api_attachment_lifecycle_recovery_suites.h"
#include "api/kernel_api_attachment_lifecycle_suites.h"

void run_attachment_lifecycle_recovery_tests() {
  run_attachment_lifecycle_recovery_startup_tests();
  run_attachment_lifecycle_recovery_reopen_tests();
}
