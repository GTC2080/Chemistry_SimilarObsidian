// Reason: Keep the deterministic retrosynthesis rule mock in the kernel so
// hosts do not own chemistry workflow rules.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace kernel::chemistry {

struct RetroPrecursor {
  std::string id;
  std::string smiles;
  std::string role;
};

struct RetroPathway {
  std::string target_id;
  std::string reaction_name;
  std::string conditions;
  std::vector<RetroPrecursor> precursors;
};

struct RetroTree {
  std::vector<RetroPathway> pathways;
};

bool generate_mock_retrosynthesis(
    std::string target_smiles,
    std::uint8_t depth,
    RetroTree& out_tree);

}  // namespace kernel::chemistry
