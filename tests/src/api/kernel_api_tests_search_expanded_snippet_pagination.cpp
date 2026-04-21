// Reason: This file keeps the expanded search snippet/pagination suite composition thin while concrete regressions live in smaller topic files.

#include "api/kernel_api_search_expanded_snippet_pagination_suites.h"
#include "api/kernel_api_search_expanded_suites.h"

void run_search_expanded_snippet_pagination_tests() {
  run_search_expanded_snippet_tests();
  run_search_expanded_pagination_tests();
}
