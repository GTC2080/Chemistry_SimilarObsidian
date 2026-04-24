// Reason: This file exposes spectroscopy data parsing through the kernel C
// ABI so Tauri Rust only bridges file bytes into shared chemistry logic.

#include "kernel/c_api.h"

#include "chemistry/spectroscopy_parser.h"
#include "core/kernel_shared.h"

#include <algorithm>
#include <new>
#include <string_view>

namespace {

void reset_spectroscopy_data_impl(kernel_spectroscopy_data* data) {
  if (data == nullptr) {
    return;
  }

  delete[] data->x;
  if (data->series != nullptr) {
    for (std::size_t index = 0; index < data->series_count; ++index) {
      delete[] data->series[index].y;
      delete[] data->series[index].label;
    }
    delete[] data->series;
  }
  delete[] data->x_label;
  delete[] data->title;

  data->x = nullptr;
  data->x_count = 0;
  data->series = nullptr;
  data->series_count = 0;
  data->x_label = nullptr;
  data->title = nullptr;
  data->is_nmr = 0;
  data->error = KERNEL_SPECTROSCOPY_PARSE_ERROR_NONE;
}

bool fill_spectroscopy_data(
    const kernel::chemistry::SpectroscopyParseData& source,
    kernel_spectroscopy_data* out_data) {
  if (source.x.empty() || source.series.empty()) {
    return false;
  }

  out_data->x = new (std::nothrow) double[source.x.size()];
  if (out_data->x == nullptr) {
    return false;
  }
  out_data->x_count = source.x.size();
  std::copy(source.x.begin(), source.x.end(), out_data->x);

  out_data->series =
      new (std::nothrow) kernel_spectrum_series[source.series.size()];
  if (out_data->series == nullptr) {
    return false;
  }
  out_data->series_count = source.series.size();

  for (std::size_t index = 0; index < source.series.size(); ++index) {
    const auto& source_series = source.series[index];
    auto& out_series = out_data->series[index];
    out_series.y = nullptr;
    out_series.count = 0;
    out_series.label = nullptr;

    if (!source_series.y.empty()) {
      out_series.y = new (std::nothrow) double[source_series.y.size()];
      if (out_series.y == nullptr) {
        return false;
      }
      out_series.count = source_series.y.size();
      std::copy(source_series.y.begin(), source_series.y.end(), out_series.y);
    }

    out_series.label = kernel::core::duplicate_c_string(source_series.label);
    if (out_series.label == nullptr) {
      return false;
    }
  }

  out_data->x_label = kernel::core::duplicate_c_string(source.x_label);
  out_data->title = kernel::core::duplicate_c_string(source.title);
  if (out_data->x_label == nullptr || out_data->title == nullptr) {
    return false;
  }

  out_data->is_nmr = source.is_nmr ? 1 : 0;
  out_data->error = KERNEL_SPECTROSCOPY_PARSE_ERROR_NONE;
  return true;
}

}  // namespace

extern "C" kernel_status kernel_parse_spectroscopy_text(
    const char* raw,
    const size_t raw_size,
    const char* extension,
    kernel_spectroscopy_data* out_data) {
  reset_spectroscopy_data_impl(out_data);
  if (raw == nullptr || extension == nullptr || out_data == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const auto parsed =
      kernel::chemistry::parse_spectroscopy_text(std::string_view(raw, raw_size), extension);
  if (parsed.error != KERNEL_SPECTROSCOPY_PARSE_ERROR_NONE) {
    out_data->error = parsed.error;
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  if (!fill_spectroscopy_data(parsed.data, out_data)) {
    reset_spectroscopy_data_impl(out_data);
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }

  return kernel::core::make_status(KERNEL_OK);
}

extern "C" void kernel_free_spectroscopy_data(kernel_spectroscopy_data* data) {
  reset_spectroscopy_data_impl(data);
}
