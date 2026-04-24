// Reason: This file isolates host-facing spectroscopy text parsing in the
// chemistry kernel while Tauri keeps file IO and JSON marshalling.

#pragma once

#include "kernel/types.h"

#include <string>
#include <string_view>
#include <vector>

namespace kernel::chemistry {

struct SpectroscopySeries {
  std::vector<double> y;
  std::string label;
};

struct SpectroscopyParseData {
  std::vector<double> x;
  std::vector<SpectroscopySeries> series;
  std::string x_label;
  std::string title;
  bool is_nmr = false;
};

struct SpectroscopyParseResult {
  kernel_spectroscopy_parse_error error = KERNEL_SPECTROSCOPY_PARSE_ERROR_NONE;
  SpectroscopyParseData data;
};

SpectroscopyParseResult parse_spectroscopy_text(
    std::string_view raw,
    std::string_view extension);

}  // namespace kernel::chemistry
