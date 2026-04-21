// Reason: Keep the search test runner thin while concrete regressions live in topic-focused files.

#include "search/search_test_suites.h"

#include <iostream>
#include <stdexcept>
int main() {
  try {
    run_search_note_core_tests();
    run_search_snippet_pagination_tests();
    run_search_ranking_tests();
    run_search_incremental_refresh_tests();
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "search_tests failed: " << ex.what() << "\n";
    return 1;
  }
}
