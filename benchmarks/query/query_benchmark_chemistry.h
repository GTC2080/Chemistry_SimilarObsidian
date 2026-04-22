// Reason: Keep Track 5 chemistry-query benchmark coverage isolated so the
// main query benchmark file does not keep swelling as new chemistry surfaces
// land.

#pragma once

#include "kernel/c_api.h"

#include <filesystem>
#include <string>

namespace kernel::benchmarks::query {

struct ChemistryBenchmarkConfig {
  std::string metadata_rel_path;
  std::string catalog_first_rel_path;
  std::string lookup_rel_path;
  std::string note_source_rel_path;
  std::string referrer_first_note_rel_path;
};

bool prepare_chemistry_query_benchmark_fixture(
    const std::filesystem::path& vault_root,
    kernel_handle* handle,
    ChemistryBenchmarkConfig& out_config);

bool run_chemistry_query_benchmarks(
    kernel_handle* handle,
    const ChemistryBenchmarkConfig& config,
    int iterations);

}  // namespace kernel::benchmarks::query
