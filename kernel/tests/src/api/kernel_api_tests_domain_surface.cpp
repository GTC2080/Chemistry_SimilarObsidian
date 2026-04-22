// Reason: Keep Track 4 domain-surface entry wiring thin so attachment- and
// PDF-carrier metadata coverage stay separated.

#include "api/kernel_api_domain_surface_suites.h"

void run_domain_surface_tests() {
  run_domain_surface_attachment_tests();
  run_domain_surface_pdf_tests();
}
