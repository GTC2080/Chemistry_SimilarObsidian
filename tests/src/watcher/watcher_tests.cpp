// Reason: Keep the watcher test runner thin while concrete regressions live in topic-focused files.

#include "watcher/watcher_test_suites.h"

#include <iostream>
#include <stdexcept>

int main() {
  try {
    run_watcher_event_coalescing_tests();
    run_watcher_session_tests();
    run_watcher_refresh_integration_tests();
    run_watcher_continuity_integration_tests();
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "watcher_tests failed: " << ex.what() << "\n";
    return 1;
  }
}
