// Reason: Keep attachment public-surface entry wiring thin so catalog and referrer coverage stay separated.

#include "api/kernel_api_attachment_public_surface_suites.h"

void run_attachment_public_surface_tests() {
  run_attachment_public_surface_catalog_tests();
  run_attachment_public_surface_refs_tests();
}
