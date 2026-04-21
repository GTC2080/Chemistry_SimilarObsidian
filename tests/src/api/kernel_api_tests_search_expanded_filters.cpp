// Reason: This file keeps the expanded search filter suite composition thin while surface and validation regressions live in smaller topic files.

#include "api/kernel_api_search_expanded_filter_suites.h"
#include "api/kernel_api_search_expanded_suites.h"

void run_search_expanded_filter_attachment_tests() {
  run_search_expanded_filter_surface_tests();
  run_search_expanded_filter_validation_tests();
}
