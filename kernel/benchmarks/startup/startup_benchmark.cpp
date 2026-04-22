// Reason: This file provides the first repeatable startup timing loop for open/close of a vault session.

#include "kernel/c_api.h"
#include "benchmarks/benchmark_thresholds.h"
#include "recovery/journal.h"
#include "vault/state_paths.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {

bool expect_ok(const kernel_status status, const char* operation) {
  if (status.code == KERNEL_OK) {
    return true;
  }
  std::cerr << operation << " failed with code " << status.code << "\n";
  return false;
}

bool prepare_recovery_vault(const std::filesystem::path& vault) {
  std::filesystem::create_directories(vault);

  kernel_handle* handle = nullptr;
  if (!expect_ok(kernel_open_vault(vault.string().c_str(), &handle), "prepare open")) {
    return false;
  }
  if (!expect_ok(kernel_close(handle), "prepare close")) {
    return false;
  }

  const auto state_dir = kernel::vault::state_dir_for_vault(vault);
  const auto journal_path = kernel::vault::recovery_journal_path(state_dir);
  const auto target_path = vault / "recovered.md";
  const auto temp_path = vault / "recovered.md.benchmark.tmp";

  {
    std::ofstream output(target_path, std::ios::binary | std::ios::trunc);
    if (!output) {
      std::cerr << "target seed write failed\n";
      return false;
    }
    output << "recovered body";
  }
  {
    std::ofstream output(temp_path, std::ios::binary | std::ios::trunc);
    if (!output) {
      std::cerr << "temp seed write failed\n";
      return false;
    }
    output << "stale temp";
  }

  const std::error_code journal_ec = kernel::recovery::append_save_begin(
      journal_path,
      "bench-recovery-op",
      "recovered.md",
      temp_path);
  if (journal_ec) {
    std::cerr << "append_save_begin failed\n";
    return false;
  }

  return true;
}

}  // namespace

int main() {
  const auto base = std::filesystem::temp_directory_path() / "chem_kernel_startup_bench";
  const auto clean_vault = base / "clean";
  std::filesystem::create_directories(clean_vault);

  constexpr int clean_iterations = 100;
  const auto clean_start = std::chrono::steady_clock::now();

  for (int i = 0; i < clean_iterations; ++i) {
    kernel_handle* handle = nullptr;
    if (!expect_ok(kernel_open_vault(clean_vault.string().c_str(), &handle), "clean open")) {
      return 1;
    }
    if (!expect_ok(kernel_close(handle), "clean close")) {
      return 1;
    }
  }

  const auto clean_end = std::chrono::steady_clock::now();
  const auto clean_elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(clean_end - clean_start).count();

  constexpr int recovery_iterations = 25;
  const auto recovery_start = std::chrono::steady_clock::now();
  for (int i = 0; i < recovery_iterations; ++i) {
    const auto recovery_vault = base / ("recovery_" + std::to_string(i));
    if (!prepare_recovery_vault(recovery_vault)) {
      return 1;
    }

    kernel_handle* handle = nullptr;
    if (!expect_ok(kernel_open_vault(recovery_vault.string().c_str(), &handle), "recovery open")) {
      return 1;
    }
    if (!expect_ok(kernel_close(handle), "recovery close")) {
      return 1;
    }

    std::filesystem::remove_all(recovery_vault);
    std::filesystem::remove_all(kernel::vault::state_dir_for_vault(recovery_vault));
  }
  const auto recovery_end = std::chrono::steady_clock::now();
  const auto recovery_elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(recovery_end - recovery_start).count();

  const bool clean_within_gate =
      kernel::benchmarks::report_gate(kernel::benchmarks::kStartupCleanGate, clean_elapsed_ms);
  const bool recovery_within_gate =
      kernel::benchmarks::report_gate(kernel::benchmarks::kStartupRecoveryGate, recovery_elapsed_ms);

  std::cout << " startup_benchmark clean_iterations=" << clean_iterations
            << " recovery_iterations=" << recovery_iterations
            << " gate_passed=" << (clean_within_gate && recovery_within_gate ? "true" : "false")
            << "\n";

  std::filesystem::remove_all(base);
  std::filesystem::remove_all(kernel::vault::state_dir_for_vault(clean_vault));
  return clean_within_gate && recovery_within_gate ? 0 : 1;
}
