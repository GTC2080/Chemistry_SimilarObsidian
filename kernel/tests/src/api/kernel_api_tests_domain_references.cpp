// Reason: Keep Track 4 domain-reference entry wiring thin so note-side and
// referrer-side coverage stay separated.

#include "api/kernel_api_domain_reference_suites.h"

void run_domain_reference_tests() {
  run_domain_reference_note_ref_tests();
  run_domain_reference_referrer_tests();
}
