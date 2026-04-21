// Reason: This file keeps the shared runtime-only helpers out of the public kernel facade files.

#pragma once

#include "core/kernel_internal.h"
#include "kernel/types.h"

#include <chrono>
#include <cstdint>
#include <stop_token>

namespace kernel::core {

bool rebuild_in_progress(kernel_handle* handle);
void join_completed_background_rebuild_if_needed(kernel_handle* handle);
void sleep_with_stop(std::stop_token stop_token, std::chrono::milliseconds duration);
void watcher_loop(std::stop_token stop_token, kernel_handle* handle);
kernel_error_code run_rebuild(kernel_handle* handle, std::uint64_t rebuild_started_at_ns);

}  // namespace kernel::core
