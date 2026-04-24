// Reason: This file owns the deterministic mock retrosynthesis rules that
// used to live in the Tauri Rust backend.

#include "chemistry/retrosynthesis.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <deque>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_set>

namespace kernel::chemistry {
namespace {

std::string normalize_smiles(std::string_view smiles) {
  std::string normalized;
  normalized.reserve(smiles.size());
  for (const unsigned char ch : smiles) {
    if (!std::isspace(ch)) {
      normalized.push_back(static_cast<char>(ch));
    }
  }
  return normalized;
}

std::string hex_u64(const std::uint64_t value) {
  std::ostringstream out;
  out << std::hex << value;
  return out.str();
}

std::uint64_t fnv1a_64(std::string_view value) {
  std::uint64_t hash = 14695981039346656037ull;
  for (const unsigned char ch : value) {
    hash ^= ch;
    hash *= 1099511628211ull;
  }
  return hash;
}

std::string node_id_from_smiles(std::string_view smiles) {
  return "retro_" + hex_u64(fnv1a_64(normalize_smiles(smiles)));
}

RetroPrecursor precursor(std::string smiles, std::string role) {
  RetroPrecursor result;
  result.id = node_id_from_smiles(smiles);
  result.smiles = std::move(smiles);
  result.role = std::move(role);
  return result;
}

RetroPathway fallback_pathway(std::string_view smiles) {
  RetroPathway pathway;
  pathway.target_id = node_id_from_smiles(smiles);
  pathway.reaction_name = "Generic Bond Disconnection";
  pathway.conditions = "Base-mediated two-component assembly";
  pathway.precursors.push_back(precursor("C1=CC=CC=C1", "reactant"));
  pathway.precursors.push_back(precursor("O=C(O)C", "reactant"));
  pathway.precursors.push_back(precursor("K2CO3", "reagent"));
  return pathway;
}

RetroPathway infer_mock_pathway(std::string_view smiles) {
  const std::string normalized = normalize_smiles(smiles);
  RetroPathway pathway;
  pathway.target_id = node_id_from_smiles(normalized);

  if (normalized.find("C(=O)N") != std::string::npos) {
    pathway.reaction_name = "Amide Coupling";
    pathway.conditions = "EDC.HCl, DIPEA, DMF, rt";
    pathway.precursors.push_back(precursor("O=C(O)C1=CC=CC=C1", "reactant"));
    pathway.precursors.push_back(precursor("NCC1=CC=CC=C1", "reactant"));
    pathway.precursors.push_back(precursor("HATU", "reagent"));
    return pathway;
  }

  if (normalized.find("C(=O)O") != std::string::npos) {
    pathway.reaction_name = "Fischer Esterification";
    pathway.conditions = "H2SO4 (cat.), EtOH, reflux";
    pathway.precursors.push_back(precursor("O=C(O)C1=CC=CC=C1", "reactant"));
    pathway.precursors.push_back(precursor("CCO", "reactant"));
    return pathway;
  }

  if (
      normalized.find("Br") != std::string::npos ||
      normalized.find('I') != std::string::npos ||
      normalized.find("B(") != std::string::npos) {
    pathway.reaction_name = "Suzuki-Miyaura Coupling";
    pathway.conditions = "Pd(PPh3)4, K2CO3, THF, 80C";
    pathway.precursors.push_back(precursor("B(O)Oc1ccccc1", "reactant"));
    pathway.precursors.push_back(precursor("Brc1ccccc1", "reactant"));
    pathway.precursors.push_back(precursor("[Pd]", "catalyst"));
    return pathway;
  }

  return fallback_pathway(normalized);
}

}  // namespace

bool generate_mock_retrosynthesis(
    std::string target_smiles,
    const std::uint8_t depth,
    RetroTree& out_tree) {
  out_tree.pathways.clear();

  std::string root_smiles = normalize_smiles(target_smiles);
  if (root_smiles.empty()) {
    return false;
  }

  const std::uint8_t max_depth = std::clamp<std::uint8_t>(
      depth,
      static_cast<std::uint8_t>(1),
      static_cast<std::uint8_t>(4));
  std::deque<std::tuple<std::string, std::string, std::uint8_t>> queue;
  std::unordered_set<std::string> expanded;

  queue.emplace_back(
      node_id_from_smiles(root_smiles),
      std::move(root_smiles),
      static_cast<std::uint8_t>(0));

  while (!queue.empty()) {
    auto [target_id, smiles, level] = std::move(queue.front());
    queue.pop_front();

    if (level >= max_depth || expanded.find(target_id) != expanded.end()) {
      continue;
    }
    expanded.insert(target_id);

    RetroPathway pathway = infer_mock_pathway(smiles);
    pathway.target_id = target_id;
    out_tree.pathways.push_back(pathway);

    const std::uint8_t next_level = static_cast<std::uint8_t>(level + 1);
    if (next_level >= max_depth) {
      continue;
    }

    for (const auto& candidate : out_tree.pathways.back().precursors) {
      if (candidate.role == "reactant") {
        queue.emplace_back(candidate.id, candidate.smiles, next_level);
      }
    }
  }

  return !out_tree.pathways.empty();
}

}  // namespace kernel::chemistry
