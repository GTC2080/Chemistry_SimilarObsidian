// Reason: This file keeps the core contract suite composition thin while concrete contract coverage lives in topic-specific files.

#include "api/kernel_api_core_base_suites.h"
#include "api/kernel_api_core_contract_suites.h"

void run_kernel_api_core_base_contract_tests() {
  run_kernel_api_core_state_contract_tests();
  run_kernel_api_core_write_contract_tests();
  run_kernel_api_core_parser_contract_tests();
  run_kernel_api_core_note_catalog_contract_tests();
  run_kernel_api_core_file_tree_contract_tests();
  run_kernel_api_core_tag_contract_tests();
  run_kernel_api_core_graph_contract_tests();
  run_kernel_api_core_vault_entry_contract_tests();
}

void run_kernel_api_core_contract_tests() {
  run_kernel_api_core_base_contract_tests();
  run_kernel_api_attachment_legacy_contract_tests();
}
