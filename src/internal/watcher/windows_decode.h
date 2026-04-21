// Reason: This file defines the narrow Win32 notify-buffer decode seam before any live watcher loop is introduced.

#pragma once

#include "watcher/watcher.h"

#include <cstddef>
#include <vector>

namespace kernel::watcher {

std::vector<RawChangeEvent> decode_win32_notify_buffer(const std::byte* bytes, std::size_t size);
RawChangeEvent make_overflow_event();

}  // namespace kernel::watcher
