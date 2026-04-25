// Reason: Keep the top-level API test target as a thin runner so detailed suites can stay focused.

#include "kernel/c_api.h"

#include "api/kernel_api_test_suites.h"
#include "api/kernel_api_test_support.h"
#include "support/test_support.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void test_phase1_alpha_smoke_flow() {
  const auto vault = make_temp_vault();
  const auto export_path = vault / "alpha-smoke-diagnostics.json";

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "alpha smoke should start from a ready index state");

  const std::string initial_content =
      "# Alpha Smoke Title\n"
      "alpha-smoke-before-token\n"
      "#alphasmoke\n"
      "[[AlphaSmokeLink]]\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "alpha-smoke.md",
      initial_content.data(),
      initial_content.size(),
      nullptr,
      &metadata,
      &disposition));
  require_true(disposition == KERNEL_WRITE_WRITTEN, "alpha smoke write should persist note content");

  kernel_search_results results{};
  expect_ok(kernel_search_notes(handle, "alpha-smoke-before-token", &results));
  require_true(results.count == 1, "alpha smoke initial search should find the written note");
  require_true(std::string(results.hits[0].rel_path) == "alpha-smoke.md", "alpha smoke initial search should preserve rel_path");
  kernel_free_search_results(&results);

  write_file_bytes(
      vault / "alpha-smoke.md",
      "# Alpha Smoke Title Updated\n"
      "alpha-smoke-after-token\n"
      "#alphasmokeupdated\n"
      "[[AlphaSmokeLinkUpdated]]\n");

  require_eventually(
      [&]() {
        kernel_search_results old_results{};
        if (kernel_search_notes(handle, "alpha-smoke-before-token", &old_results).code != KERNEL_OK) {
          return false;
        }
        const bool old_gone = old_results.count == 0;
        kernel_free_search_results(&old_results);

        kernel_search_results new_results{};
        if (kernel_search_notes(handle, "alpha-smoke-after-token", &new_results).code != KERNEL_OK) {
          return false;
        }
        const bool new_present =
            new_results.count == 1 &&
            std::string(new_results.hits[0].rel_path) == "alpha-smoke.md";
        kernel_free_search_results(&new_results);

        return old_gone && new_present;
      },
      "alpha smoke watcher path should reconcile external modify");

  expect_ok(kernel_rebuild_index(handle));

  kernel_state_snapshot snapshot{};
  expect_ok(kernel_get_state(handle, &snapshot));
  require_true(snapshot.index_state == KERNEL_INDEX_READY, "alpha smoke rebuild should leave the index ready");
  require_true(snapshot.indexed_note_count == 1, "alpha smoke rebuild should preserve one active note");

  expect_ok(kernel_export_diagnostics(handle, export_path.string().c_str()));
  const std::string exported = read_file_text(export_path);
  require_true(exported.find("\"index_state\":\"READY\"") != std::string::npos, "alpha smoke diagnostics should export READY state");
  require_true(exported.find("\"index_fault_reason\":\"\"") != std::string::npos, "alpha smoke diagnostics should export a cleared fault reason");
  require_true(exported.find("\"indexed_note_count\":1") != std::string::npos, "alpha smoke diagnostics should export the active note count");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

int main() {
  try {
    run_kernel_api_core_contract_tests();
    run_runtime_recovery_tests();
    run_attachment_public_surface_tests();
    run_attachment_lifecycle_tests();
    run_chemistry_tests();
    run_crystal_compute_tests();
    run_symmetry_compute_tests();
    run_domain_surface_tests();
    run_domain_object_tests();
    run_domain_reference_tests();
    run_domain_diagnostics_tests();
    run_pdf_surface_tests();
    run_product_compute_tests();
    run_search_public_surface_tests();
    run_kernel_api_watcher_smoke_tests();
    run_rebuild_runtime_tests();
    // Attachment rebuild missing-state coverage now lives with the attachment lifecycle cluster.
    run_attachment_diagnostics_tests();
    run_pdf_diagnostics_tests();
    run_runtime_diagnostics_tests();
    test_phase1_alpha_smoke_flow();
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "kernel_api_tests failed: " << ex.what() << "\n";
    return 1;
  }
}
