// Reason: This file exposes retrosynthesis rule generation through the kernel
// C ABI so Tauri Rust only marshals command DTOs.

#include "kernel/c_api.h"

#include "chemistry/retrosynthesis.h"
#include "core/kernel_shared.h"

#include <new>
#include <string>

namespace {

void reset_retro_precursor(kernel_retro_precursor* precursor) {
  if (precursor == nullptr) {
    return;
  }
  delete[] precursor->id;
  delete[] precursor->smiles;
  delete[] precursor->role;
  precursor->id = nullptr;
  precursor->smiles = nullptr;
  precursor->role = nullptr;
}

void reset_retro_pathway(kernel_retro_pathway* pathway) {
  if (pathway == nullptr) {
    return;
  }
  delete[] pathway->target_id;
  delete[] pathway->reaction_name;
  delete[] pathway->conditions;
  if (pathway->precursors != nullptr) {
    for (std::size_t index = 0; index < pathway->precursor_count; ++index) {
      reset_retro_precursor(&pathway->precursors[index]);
    }
    delete[] pathway->precursors;
  }
  pathway->target_id = nullptr;
  pathway->reaction_name = nullptr;
  pathway->conditions = nullptr;
  pathway->precursors = nullptr;
  pathway->precursor_count = 0;
}

void reset_retro_tree_impl(kernel_retro_tree* tree) {
  if (tree == nullptr) {
    return;
  }
  if (tree->pathways != nullptr) {
    for (std::size_t index = 0; index < tree->pathway_count; ++index) {
      reset_retro_pathway(&tree->pathways[index]);
    }
    delete[] tree->pathways;
  }
  tree->pathways = nullptr;
  tree->pathway_count = 0;
}

bool fill_retro_precursor(
    const kernel::chemistry::RetroPrecursor& source,
    kernel_retro_precursor* target) {
  target->id = kernel::core::duplicate_c_string(source.id);
  target->smiles = kernel::core::duplicate_c_string(source.smiles);
  target->role = kernel::core::duplicate_c_string(source.role);
  return target->id != nullptr && target->smiles != nullptr && target->role != nullptr;
}

bool fill_retro_pathway(
    const kernel::chemistry::RetroPathway& source,
    kernel_retro_pathway* target) {
  target->target_id = kernel::core::duplicate_c_string(source.target_id);
  target->reaction_name = kernel::core::duplicate_c_string(source.reaction_name);
  target->conditions = kernel::core::duplicate_c_string(source.conditions);
  if (
      target->target_id == nullptr || target->reaction_name == nullptr ||
      target->conditions == nullptr) {
    return false;
  }

  if (source.precursors.empty()) {
    return true;
  }

  target->precursors = new (std::nothrow) kernel_retro_precursor[source.precursors.size()]{};
  if (target->precursors == nullptr) {
    return false;
  }
  target->precursor_count = source.precursors.size();
  for (std::size_t index = 0; index < source.precursors.size(); ++index) {
    if (!fill_retro_precursor(source.precursors[index], &target->precursors[index])) {
      return false;
    }
  }
  return true;
}

bool fill_retro_tree(
    const kernel::chemistry::RetroTree& source,
    kernel_retro_tree* out_tree) {
  if (source.pathways.empty()) {
    return true;
  }

  out_tree->pathways = new (std::nothrow) kernel_retro_pathway[source.pathways.size()]{};
  if (out_tree->pathways == nullptr) {
    return false;
  }
  out_tree->pathway_count = source.pathways.size();

  for (std::size_t index = 0; index < source.pathways.size(); ++index) {
    if (!fill_retro_pathway(source.pathways[index], &out_tree->pathways[index])) {
      return false;
    }
  }
  return true;
}

}  // namespace

extern "C" kernel_status kernel_generate_mock_retrosynthesis(
    const char* target_smiles,
    const std::uint8_t depth,
    kernel_retro_tree* out_tree) {
  if (out_tree == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  reset_retro_tree_impl(out_tree);
  if (target_smiles == nullptr || target_smiles[0] == '\0') {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel::chemistry::RetroTree tree;
  if (!kernel::chemistry::generate_mock_retrosynthesis(
          std::string(target_smiles),
          depth,
          tree)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  if (!fill_retro_tree(tree, out_tree)) {
    reset_retro_tree_impl(out_tree);
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }

  return kernel::core::make_status(KERNEL_OK);
}

extern "C" void kernel_free_retro_tree(kernel_retro_tree* tree) {
  reset_retro_tree_impl(tree);
}
