// Reason: Keep Track 4 domain-object entry wiring thin so attachment- and PDF-
// subtype coverage stay separated.

#include "api/kernel_api_domain_object_suites.h"

void run_domain_object_tests() {
  run_domain_object_attachment_tests();
  run_domain_object_pdf_tests();
}
