// Reason: This file keeps the attachment watcher lifecycle suite composition thin while concrete regressions live in topic-specific files.

#include "api/kernel_api_attachment_lifecycle_suites.h"
#include "api/kernel_api_attachment_lifecycle_watcher_suites.h"

void run_attachment_lifecycle_watcher_tests() {
  run_attachment_lifecycle_watcher_backoff_tests();
  run_attachment_lifecycle_watcher_rescan_tests();
}
