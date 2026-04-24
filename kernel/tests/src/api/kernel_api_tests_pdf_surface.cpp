// Reason: Keep PDF public-surface entry wiring thin so metadata coverage stays
// separate from future anchor and reference-surface suites.

#include "api/kernel_api_pdf_surface_suites.h"

void run_pdf_surface_tests() {
  run_pdf_surface_metadata_tests();
  run_pdf_surface_anchor_tests();
  run_pdf_surface_ink_tests();
  run_pdf_surface_reference_tests();
}
