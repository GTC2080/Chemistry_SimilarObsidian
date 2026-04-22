// Reason: Keep the note-core search suite composition thin while concrete regressions live in smaller topic files.

#include "search/search_note_core_suites.h"
#include "search/search_test_suites.h"

void run_search_note_core_tests() {
  run_search_note_persistence_tests();
  run_search_note_query_contract_tests();
}
