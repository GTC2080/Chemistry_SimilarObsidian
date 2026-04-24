// Reason: Keep retrosynthesis coverage at the kernel ABI boundary so Rust
// command handlers do not retain chemistry workflow rules.

#include "kernel/c_api.h"

#include "api/kernel_api_chemistry_suites.h"
#include "support/test_support.h"

#include <string>
#include <string_view>

namespace {

void require_ok_status(const kernel_status status, std::string_view context) {
  require_true(
      status.code == KERNEL_OK,
      std::string(context) + ": expected KERNEL_OK, got status " +
          std::to_string(status.code));
}

void test_retrosynthesis_generates_amide_pathway() {
  kernel_retro_tree tree{};
  require_ok_status(
      kernel_generate_mock_retrosynthesis(" CC(=O)NCC1=CC=CC=C1 ", 2, &tree),
      "retrosynthesis amide");

  require_true(tree.pathway_count >= 1, "retrosynthesis should emit at least one pathway");
  require_true(tree.pathways != nullptr, "retrosynthesis should allocate pathways");
  require_true(
      std::string(tree.pathways[0].reaction_name) == "Amide Coupling",
      "retrosynthesis should classify amide coupling");
  require_true(
      std::string(tree.pathways[0].target_id).rfind("retro_", 0) == 0,
      "retrosynthesis target ids should use the retro prefix");
  require_true(
      tree.pathways[0].precursor_count == 3,
      "amide coupling should expose two reactants and one reagent");
  require_true(
      std::string(tree.pathways[0].precursors[2].role) == "reagent",
      "amide coupling should preserve reagent role");

  kernel_free_retro_tree(&tree);
  require_true(
      tree.pathways == nullptr && tree.pathway_count == 0,
      "retrosynthesis free should reset the tree");
}

void test_retrosynthesis_clamps_depth_and_rejects_empty_target() {
  kernel_retro_tree tree{};
  require_ok_status(
      kernel_generate_mock_retrosynthesis("C(=O)O", 99, &tree),
      "retrosynthesis depth clamp");
  require_true(
      tree.pathway_count >= 1,
      "retrosynthesis clamped depth should still emit pathways");
  kernel_free_retro_tree(&tree);

  require_true(
      kernel_generate_mock_retrosynthesis("   ", 2, &tree).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "retrosynthesis should reject empty targets");
  require_true(
      kernel_generate_mock_retrosynthesis(nullptr, 2, &tree).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "retrosynthesis should reject null targets");
  require_true(
      kernel_generate_mock_retrosynthesis("CC", 2, nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "retrosynthesis should reject null output");
}

}  // namespace

void run_chemistry_retrosynthesis_tests() {
  test_retrosynthesis_generates_amide_pathway();
  test_retrosynthesis_clamps_depth_and_rejects_empty_target();
}
