// Reason: This file keeps the legacy note-search suite composition thin while concrete regressions live in smaller topic files.

#include "api/kernel_api_search_legacy_note_suites.h"
#include "api/kernel_api_search_legacy_suites.h"

void run_search_legacy_note_tests() {
  run_search_legacy_note_basic_tests();
  run_search_legacy_note_surface_tests();
}
