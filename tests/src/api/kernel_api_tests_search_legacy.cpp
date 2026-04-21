// Reason: This file keeps the legacy search suite composition thin while note and graph regressions live in topic-specific files.

#include "api/kernel_api_search_legacy_suites.h"
#include "api/kernel_api_test_suites.h"

void run_search_legacy_tests() {
  run_search_legacy_note_tests();
  run_search_legacy_graph_tests();
}
