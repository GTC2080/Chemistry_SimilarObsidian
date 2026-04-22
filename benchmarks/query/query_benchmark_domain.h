// Reason: Keep Track 4 domain-query benchmark coverage isolated so the core
// query benchmark file does not keep swelling as new public surfaces land.

#pragma once

#include "kernel/c_api.h"

namespace kernel::benchmarks::query {

struct DomainBenchmarkConfig {
  const char* attachment_rel_path = nullptr;
  const char* attachment_domain_object_key = nullptr;
  const char* pdf_rel_path = nullptr;
  const char* pdf_domain_object_key = nullptr;
  const char* note_source_rel_path = nullptr;
};

bool run_domain_query_benchmarks(
    kernel_handle* handle,
    const DomainBenchmarkConfig& config,
    int iterations);

}  // namespace kernel::benchmarks::query
