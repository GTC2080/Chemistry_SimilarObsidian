// Reason: Keep PDF diagnostics entry wiring thin so snapshot coverage can grow
// without bloating the top-level kernel_api_tests runner.

#include "api/kernel_api_pdf_diagnostics_suites.h"

void run_pdf_diagnostics_tests() {
  run_pdf_diagnostics_snapshot_tests();
}
