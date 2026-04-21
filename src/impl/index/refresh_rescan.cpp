// Reason: This file owns full-rescan orchestration and the fault-injection seams used by tests.

#include "index/refresh.h"

#include "index/refresh_internal.h"

#include "platform/platform.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>
#include <unordered_set>

namespace kernel::index {
namespace {

std::atomic<int> g_injected_full_rescan_failures{0};
std::atomic<int> g_injected_full_rescan_error_value{0};
std::atomic<int> g_injected_full_rescan_delays{0};
std::atomic<int> g_injected_full_rescan_delay_ms{0};
std::atomic<int> g_injected_full_rescan_interrupt_after_refresh_phase{0};

}  // namespace

bool stop_requested(const std::stop_token stop_token) {
  return stop_token.stop_possible() && stop_token.stop_requested();
}

void inject_full_rescan_failures(std::errc error, int remaining_failures) {
  g_injected_full_rescan_error_value.store(static_cast<int>(error), std::memory_order_relaxed);
  g_injected_full_rescan_failures.store(
      remaining_failures > 0 ? remaining_failures : 0,
      std::memory_order_relaxed);
}

void inject_full_rescan_delay_ms(int delay_ms, int remaining_delays) {
  g_injected_full_rescan_delay_ms.store(
      delay_ms > 0 ? delay_ms : 0,
      std::memory_order_relaxed);
  g_injected_full_rescan_delays.store(
      remaining_delays > 0 ? remaining_delays : 0,
      std::memory_order_relaxed);
}

void inject_full_rescan_interrupt_after_refresh_phase(int remaining_interrupts) {
  g_injected_full_rescan_interrupt_after_refresh_phase.store(
      remaining_interrupts > 0 ? remaining_interrupts : 0,
      std::memory_order_relaxed);
}

std::error_code full_rescan_markdown_vault(
    kernel::storage::Database& db,
    const std::filesystem::path& vault_root,
    const std::stop_token stop_token) {
  int remaining_delays = g_injected_full_rescan_delays.load(std::memory_order_relaxed);
  while (remaining_delays > 0) {
    if (g_injected_full_rescan_delays.compare_exchange_weak(
            remaining_delays,
            remaining_delays - 1,
            std::memory_order_relaxed)) {
      const int delay_ms = g_injected_full_rescan_delay_ms.load(std::memory_order_relaxed);
      if (delay_ms > 0) {
        int remaining_delay_ms = delay_ms;
        while (remaining_delay_ms > 0) {
          if (stop_requested(stop_token)) {
            return std::make_error_code(std::errc::operation_canceled);
          }
          const int sleep_ms = std::min(remaining_delay_ms, 10);
          std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
          remaining_delay_ms -= sleep_ms;
        }
      }
      break;
    }
  }

  if (stop_requested(stop_token)) {
    return std::make_error_code(std::errc::operation_canceled);
  }

  int remaining_failures = g_injected_full_rescan_failures.load(std::memory_order_relaxed);
  while (remaining_failures > 0) {
    if (g_injected_full_rescan_failures.compare_exchange_weak(
            remaining_failures,
            remaining_failures - 1,
            std::memory_order_relaxed)) {
      return {g_injected_full_rescan_error_value.load(std::memory_order_relaxed),
              std::generic_category()};
    }
  }

  std::unordered_set<std::string> on_disk_note_paths;
  std::unordered_set<std::string> on_disk_attachment_paths;
  std::error_code ec;

  for (std::filesystem::recursive_directory_iterator it(vault_root, ec), end; it != end; it.increment(ec)) {
    if (stop_requested(stop_token)) {
      return std::make_error_code(std::errc::operation_canceled);
    }
    if (ec) {
      return ec;
    }
    if (!it->is_regular_file()) {
      continue;
    }

    const auto rel_path = std::filesystem::relative(it->path(), vault_root, ec).lexically_normal().generic_string();
    if (ec) {
      return ec;
    }

    if (is_markdown_rel_path(it->path())) {
      on_disk_note_paths.insert(rel_path);
    } else {
      on_disk_attachment_paths.insert(rel_path);
    }
    ec = refresh_markdown_path(db, vault_root, rel_path);
    if (ec) {
      return ec;
    }
  }
  if (ec) {
    return ec;
  }

  int remaining_interrupts =
      g_injected_full_rescan_interrupt_after_refresh_phase.load(std::memory_order_relaxed);
  while (remaining_interrupts > 0) {
    if (g_injected_full_rescan_interrupt_after_refresh_phase.compare_exchange_weak(
            remaining_interrupts,
            remaining_interrupts - 1,
            std::memory_order_relaxed)) {
      return std::make_error_code(std::errc::operation_canceled);
    }
  }

  std::vector<std::string> known_paths;
  ec = kernel::storage::list_note_paths(db, known_paths);
  if (ec) {
    return ec;
  }

  for (const auto& rel_path : known_paths) {
    if (stop_requested(stop_token)) {
      return std::make_error_code(std::errc::operation_canceled);
    }
    if (!on_disk_note_paths.contains(rel_path)) {
      ec = kernel::storage::mark_note_deleted(db, rel_path);
      if (ec) {
        return ec;
      }
    }
  }

  std::vector<std::string> known_attachment_paths;
  ec = kernel::storage::list_attachment_paths(db, known_attachment_paths);
  if (ec) {
    return ec;
  }

  for (const auto& rel_path : known_attachment_paths) {
    if (stop_requested(stop_token)) {
      return std::make_error_code(std::errc::operation_canceled);
    }
    if (!on_disk_attachment_paths.contains(rel_path)) {
      ec = kernel::storage::mark_attachment_missing(db, rel_path);
      if (ec) {
        return ec;
      }
    }
  }

  return {};
}

}  // namespace kernel::index
