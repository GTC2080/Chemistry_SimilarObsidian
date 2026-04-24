// Reason: Keep Track 5 chemistry entry wiring thin so metadata coverage stays
// separate from later subtype and source-reference suites.

#include "api/kernel_api_chemistry_suites.h"

void run_chemistry_tests() {
  run_chemistry_metadata_tests();
  run_chemistry_subtype_tests();
  run_chemistry_reference_tests();
  run_chemistry_diagnostics_tests();
  run_chemistry_kinetics_tests();
}
