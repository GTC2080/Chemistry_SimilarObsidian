// Reason: Keep Track 3 Batch 3 PDF reference-surface entry wiring thin so
// note->PDF refs and PDF->note referrers can evolve in separate suites.

#include "api/kernel_api_pdf_surface_suites.h"

void run_pdf_surface_reference_tests() {
  run_pdf_surface_reference_note_ref_tests();
  run_pdf_surface_reference_referrer_tests();
}
