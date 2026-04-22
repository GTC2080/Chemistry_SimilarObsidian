// Reason: This file provides the first read/write roundtrip timing loop for the minimal kernel ABI.

#include "kernel/c_api.h"
#include "benchmarks/benchmark_thresholds.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

namespace {

bool expect_ok(const kernel_status status, const char* operation) {
  if (status.code == KERNEL_OK) {
    return true;
  }
  std::cerr << operation << " failed with code " << status.code << "\n";
  return false;
}

bool wait_for_search_hit(
    kernel_handle* handle,
    const std::string& query,
    const std::string& rel_path,
    const std::chrono::milliseconds timeout) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    kernel_search_results results{};
    const kernel_status status = kernel_search_notes(handle, query.c_str(), &results);
    if (status.code == KERNEL_OK) {
      const bool matched =
          results.count == 1 &&
          std::string(results.hits[0].rel_path) == rel_path;
      kernel_free_search_results(&results);
      if (matched) {
        return true;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return false;
}

}  // namespace

int main() {
  const auto vault = std::filesystem::temp_directory_path() / "chem_kernel_io_bench";
  std::filesystem::create_directories(vault);

  kernel_handle* handle = nullptr;
  if (!expect_ok(kernel_open_vault(vault.string().c_str(), &handle), "open")) {
    return 1;
  }

  constexpr int iterations = 100;
  std::string content(1024, 'a');
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};

  const auto start = std::chrono::steady_clock::now();
  std::string expected_revision;
  for (int i = 0; i < iterations; ++i) {
    const char* expected = expected_revision.empty() ? nullptr : expected_revision.c_str();
    const kernel_status write_status =
        kernel_write_note(handle, "bench.md", content.data(), content.size(), expected, &metadata, &disposition);
    if (!expect_ok(write_status, "write")) {
      return 1;
    }
    expected_revision = metadata.content_revision;

    kernel_owned_buffer buffer{};
    if (!expect_ok(kernel_read_note(handle, "bench.md", &buffer, &metadata), "read")) {
      return 1;
    }
    kernel_free_buffer(&buffer);
  }

  const auto end = std::chrono::steady_clock::now();
  const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

  constexpr int external_create_iterations = 25;
  const auto external_start = std::chrono::steady_clock::now();
  for (int i = 0; i < external_create_iterations; ++i) {
    const std::string rel_path = "external-" + std::to_string(i) + ".md";
    const std::string token = "external-token-" + std::to_string(i);
    {
      std::ofstream output(vault / rel_path, std::ios::binary | std::ios::trunc);
      if (!output) {
        std::cerr << "external create write failed\n";
        return 1;
      }
      output << "# External\n" << token << "\n";
    }

    if (!wait_for_search_hit(handle, token, rel_path, std::chrono::seconds(3))) {
      std::cerr << "external create did not become searchable in time\n";
      return 1;
    }
  }
  const auto external_end = std::chrono::steady_clock::now();
  const auto external_elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(external_end - external_start).count();

  const bool io_within_gate =
      kernel::benchmarks::report_gate(kernel::benchmarks::kIoRoundtripGate, elapsed_ms);
  const bool external_within_gate =
      kernel::benchmarks::report_gate(kernel::benchmarks::kExternalCreateGate, external_elapsed_ms);

  std::cout << " io_benchmark iterations=" << iterations
            << " external_create_iterations=" << external_create_iterations
            << " gate_passed=" << (io_within_gate && external_within_gate ? "true" : "false")
            << "\n";

  kernel_close(handle);
  std::filesystem::remove_all(vault);
  return io_within_gate && external_within_gate ? 0 : 1;
}
