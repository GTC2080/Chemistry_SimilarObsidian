// Reason: This file adds a dedicated rebuild timing loop so rebuild is measured as a first-class recovery path.

#include "kernel/c_api.h"
#include "benchmarks/benchmark_thresholds.h"
#include "benchmarks/rebuild/rebuild_benchmark_chemistry.h"

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

bool wait_until_ready(kernel_handle* handle, const std::chrono::milliseconds timeout) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    kernel_state_snapshot snapshot{};
    if (kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
        snapshot.index_state == KERNEL_INDEX_READY) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return false;
}

bool seed_vault(const std::filesystem::path& vault_root, const int note_count, const int attachment_count) {
  std::filesystem::create_directories(vault_root);
  std::filesystem::create_directories(vault_root / "assets");

  for (int i = 0; i < attachment_count; ++i) {
    std::ofstream attachment_output(
        vault_root / "assets" / ("attachment-" + std::to_string(i) + ".bin"),
        std::ios::binary | std::ios::trunc);
    if (!attachment_output) {
      std::cerr << "seed write failed for attachment-" << i << "\n";
      return false;
    }
    attachment_output << "attachment-benchmark-bytes-" << i;
  }

  for (int i = 0; i < note_count; ++i) {
    std::ofstream output(vault_root / ("note-" + std::to_string(i) + ".md"), std::ios::binary | std::ios::trunc);
    if (!output) {
      std::cerr << "seed write failed for note-" << i << "\n";
      return false;
    }
    output << "# Note " << i << "\n";
    output << "rebuild-benchmark-token-" << i << "\n";
    output << "#tag" << (i % 5) << "\n";
    output << "[[Link" << (i % 7) << "]]\n";
    output << "![Attachment](assets/attachment-" << (i % attachment_count) << ".bin)\n";
  }
  return true;
}

}  // namespace

int main() {
  const auto vault = std::filesystem::temp_directory_path() / "chem_kernel_rebuild_bench";
  constexpr int note_count = 64;
  constexpr int attachment_count = 64;
  constexpr int chemistry_spectrum_count = 16;
  constexpr int rebuild_iterations = 25;

  if (!seed_vault(vault, note_count, attachment_count)) {
    return 1;
  }
  if (!kernel::benchmarks::rebuild::seed_chemistry_rebuild_fixture(
          vault,
          chemistry_spectrum_count)) {
    return 1;
  }

  kernel_handle* handle = nullptr;
  if (!expect_ok(kernel_open_vault(vault.string().c_str(), &handle), "open")) {
    return 1;
  }
  if (!wait_until_ready(handle, std::chrono::seconds(5))) {
    std::cerr << "initial catch-up did not settle to READY in time\n";
    return 1;
  }

  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < rebuild_iterations; ++i) {
    if (!expect_ok(kernel_rebuild_index(handle), "rebuild")) {
      return 1;
    }
  }
  const auto end = std::chrono::steady_clock::now();
  const auto rebuild_elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

  const bool rebuild_within_gate =
      kernel::benchmarks::report_gate(kernel::benchmarks::kRebuildGate, rebuild_elapsed_ms);
  const bool chemistry_rebuild_within_gate =
      kernel::benchmarks::report_gate(
          kernel::benchmarks::kChemistryRebuildMixedSpectraDatasetGate,
          rebuild_elapsed_ms);

  std::cout << " rebuild_benchmark note_count=" << note_count
            << " attachment_count=" << attachment_count
            << " chemistry_spectrum_count=" << chemistry_spectrum_count
            << " rebuild_iterations=" << rebuild_iterations
            << " gate_passed="
            << (rebuild_within_gate && chemistry_rebuild_within_gate ? "true" : "false")
            << "\n";

  kernel_close(handle);
  std::filesystem::remove_all(vault);
  return rebuild_within_gate && chemistry_rebuild_within_gate ? 0 : 1;
}
