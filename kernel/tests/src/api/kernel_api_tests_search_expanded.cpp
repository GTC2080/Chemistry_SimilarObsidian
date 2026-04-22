// Reason: Keep expanded search entry wiring thin so snippet/pagination, filter, and ranking suites stay separated.

#include "api/kernel_api_search_expanded_suites.h"

void run_search_expanded_tests() {
  run_search_expanded_snippet_pagination_tests();
  run_search_expanded_filter_attachment_tests();
  run_search_expanded_ranking_tests();
}
