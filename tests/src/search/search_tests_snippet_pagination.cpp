// Reason: Keep the snippet/pagination search suite composition thin while concrete regressions live in smaller topic files.

#include "search/search_snippet_pagination_suites.h"
#include "search/search_test_suites.h"

void run_search_snippet_pagination_tests() {
  run_search_snippet_tests();
  run_search_page_contract_tests();
}
