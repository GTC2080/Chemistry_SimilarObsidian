// Reason: Keep spectroscopy data parsing covered at the C ABI boundary so
// Tauri can delegate spectrum reader behavior to the kernel.

#include "kernel/c_api.h"

#include "api/kernel_api_chemistry_suites.h"
#include "support/test_support.h"

#include <cmath>
#include <string>
#include <string_view>

namespace {

void require_ok_status(const kernel_status status, std::string_view context) {
  require_true(
      status.code == KERNEL_OK,
      std::string(context) + ": expected KERNEL_OK, got status " +
          std::to_string(status.code));
}

void require_near(const double actual, const double expected, std::string_view context) {
  require_true(
      std::abs(actual - expected) < 1.0e-9,
      std::string(context) + ": expected " + std::to_string(expected) + ", got " +
          std::to_string(actual));
}

void test_spectroscopy_parses_csv_with_multiple_series() {
  const std::string raw =
      "# ignored\n"
      "ppm,intensity,fit\n"
      "1.0,5.0,4.5\n"
      "2.0,6.0,bad\n"
      "3.0,7.0\n";
  kernel_spectroscopy_data data{};

  require_ok_status(
      kernel_parse_spectroscopy_text(raw.data(), raw.size(), "csv", &data),
      "spectroscopy csv parse");

  require_true(data.x_count == 3, "csv should parse three x values");
  require_true(data.series_count == 2, "csv should parse two y series");
  require_true(data.x_label != nullptr && std::string(data.x_label) == "ppm", "csv x label");
  require_true(data.title != nullptr && std::string(data.title).empty(), "csv title");
  require_true(data.is_nmr == 1, "csv ppm range should infer nmr");

  require_near(data.x[0], 1.0, "csv x[0]");
  require_near(data.x[2], 3.0, "csv x[2]");
  require_true(
      data.series[0].label != nullptr && std::string(data.series[0].label) == "intensity",
      "csv first series label");
  require_true(
      data.series[1].label != nullptr && std::string(data.series[1].label) == "fit",
      "csv second series label");
  require_true(data.series[0].count == 3, "csv first series count");
  require_true(data.series[1].count == 3, "csv second series count");
  require_near(data.series[0].y[1], 6.0, "csv y series value");
  require_near(data.series[1].y[1], 0.0, "csv invalid y fallback");
  require_near(data.series[1].y[2], 0.0, "csv missing y fallback");

  kernel_free_spectroscopy_data(&data);
  require_true(
      data.x == nullptr && data.series == nullptr && data.x_count == 0 &&
          data.series_count == 0,
      "spectroscopy free should reset csv data");
}

void test_spectroscopy_parses_jdx_peak_table() {
  const std::string raw =
      "##TITLE=Sample NMR\n"
      "##DATATYPE=NMR SPECTRUM\n"
      "##XUNITS=PPM\n"
      "##YUNITS=INTENSITY\n"
      "##PEAK TABLE=(XY..XY)\n"
      "1.0, 10.0; 2.0, 11.0\n"
      "##END=\n";
  kernel_spectroscopy_data data{};

  require_ok_status(
      kernel_parse_spectroscopy_text(raw.data(), raw.size(), "jdx", &data),
      "spectroscopy jdx parse");

  require_true(data.x_count == 2, "jdx should parse two x values");
  require_true(data.series_count == 1, "jdx should parse one y series");
  require_true(data.title != nullptr && std::string(data.title) == "Sample NMR", "jdx title");
  require_true(data.x_label != nullptr && std::string(data.x_label) == "PPM", "jdx x label");
  require_true(
      data.series[0].label != nullptr && std::string(data.series[0].label) == "INTENSITY",
      "jdx y label");
  require_true(data.is_nmr == 1, "jdx datatype should infer nmr");
  require_near(data.x[1], 2.0, "jdx x[1]");
  require_near(data.series[0].y[1], 11.0, "jdx y[1]");

  kernel_free_spectroscopy_data(&data);
}

void test_spectroscopy_reports_parse_errors_and_resets_output() {
  kernel_spectroscopy_data data{};
  const std::string bad_csv = "name,value\nnot-a-number,still-bad\n";

  kernel_status status =
      kernel_parse_spectroscopy_text(bad_csv.data(), bad_csv.size(), "csv", &data);
  require_true(
      status.code == KERNEL_ERROR_INVALID_ARGUMENT,
      "bad csv should return invalid argument");
  require_true(
      data.error == KERNEL_SPECTROSCOPY_PARSE_ERROR_CSV_NO_NUMERIC_ROWS,
      "bad csv should report no numeric rows");
  require_true(data.x == nullptr && data.series == nullptr, "bad csv should not allocate data");

  const std::string raw = "1,2\n";
  status = kernel_parse_spectroscopy_text(raw.data(), raw.size(), "txt", &data);
  require_true(
      status.code == KERNEL_ERROR_INVALID_ARGUMENT,
      "unsupported spectroscopy extension should return invalid argument");
  require_true(
      data.error == KERNEL_SPECTROSCOPY_PARSE_ERROR_UNSUPPORTED_EXTENSION,
      "unsupported extension should report parse error");

  require_true(
      kernel_parse_spectroscopy_text(nullptr, 0, "csv", &data).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "spectroscopy should reject null raw text");
  require_true(
      kernel_parse_spectroscopy_text(raw.data(), raw.size(), nullptr, &data).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "spectroscopy should reject null extension");
  require_true(
      kernel_parse_spectroscopy_text(raw.data(), raw.size(), "csv", nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "spectroscopy should reject null output");
}

}  // namespace

void run_chemistry_spectroscopy_tests() {
  test_spectroscopy_parses_csv_with_multiple_series();
  test_spectroscopy_parses_jdx_peak_table();
  test_spectroscopy_reports_parse_errors_and_resets_output();
}
