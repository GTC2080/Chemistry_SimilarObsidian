// Reason: This file owns spectroscopy CSV/JDX parsing rules previously held
// by the Tauri Rust service layer.

#include "chemistry/spectroscopy_parser.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <limits>
#include <string>

namespace kernel::chemistry {
namespace {

bool is_ascii_space(const char ch) {
  return std::isspace(static_cast<unsigned char>(ch)) != 0;
}

std::string to_lower_ascii(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
}

std::string trim_ascii(std::string_view value) {
  std::size_t start = 0;
  while (start < value.size() && is_ascii_space(value[start])) {
    ++start;
  }

  std::size_t end = value.size();
  while (end > start && is_ascii_space(value[end - 1])) {
    --end;
  }
  return std::string(value.substr(start, end - start));
}

std::vector<std::string_view> split_lines(std::string_view bytes) {
  std::vector<std::string_view> lines;
  if (bytes.empty()) {
    return lines;
  }

  std::size_t start = 0;
  while (start < bytes.size()) {
    const std::size_t end = bytes.find('\n', start);
    if (end == std::string_view::npos) {
      lines.push_back(bytes.substr(start));
      break;
    }
    std::size_t line_end = end;
    if (line_end > start && bytes[line_end - 1] == '\r') {
      --line_end;
    }
    lines.push_back(bytes.substr(start, line_end - start));
    start = end + 1;
  }
  return lines;
}

std::vector<std::string> split_cells(std::string_view line, const char delimiter) {
  std::vector<std::string> cells;
  std::size_t start = 0;
  while (start <= line.size()) {
    const std::size_t end = line.find(delimiter, start);
    const std::size_t cell_end = end == std::string_view::npos ? line.size() : end;
    cells.push_back(trim_ascii(line.substr(start, cell_end - start)));
    if (end == std::string_view::npos) {
      break;
    }
    start = end + 1;
  }
  return cells;
}

bool parse_double_full(std::string_view value, double& out_value) {
  const std::string trimmed = trim_ascii(value);
  if (trimmed.empty()) {
    return false;
  }

  char* parse_end = nullptr;
  out_value = std::strtod(trimmed.c_str(), &parse_end);
  return parse_end == trimmed.c_str() + trimmed.size();
}

bool parse_double_full(std::string_view value) {
  double ignored = 0.0;
  return parse_double_full(value, ignored);
}

std::vector<double> parse_numbers_lossy(std::string line) {
  for (char& ch : line) {
    if (ch == ',' || ch == ';' || ch == '\t') {
      ch = ' ';
    }
  }

  std::vector<double> numbers;
  std::size_t start = 0;
  while (start < line.size()) {
    while (start < line.size() && is_ascii_space(line[start])) {
      ++start;
    }
    if (start >= line.size()) {
      break;
    }

    std::size_t end = start;
    while (end < line.size() && !is_ascii_space(line[end])) {
      ++end;
    }

    double value = 0.0;
    if (parse_double_full(std::string_view(line).substr(start, end - start), value)) {
      numbers.push_back(value);
    }
    start = end;
  }
  return numbers;
}

std::vector<std::pair<double, double>> parse_numeric_pairs(std::string_view line) {
  const std::vector<double> numbers = parse_numbers_lossy(std::string(line));
  std::vector<std::pair<double, double>> pairs;
  std::size_t index = 0;
  while (index + 1 < numbers.size()) {
    pairs.emplace_back(numbers[index], numbers[index + 1]);
    index += 2;
  }
  return pairs;
}

SpectroscopyParseResult make_error(const kernel_spectroscopy_parse_error error) {
  SpectroscopyParseResult result{};
  result.error = error;
  return result;
}

SpectroscopyParseResult parse_csv_spectroscopy(std::string_view raw) {
  std::vector<std::string> lines;
  for (const std::string_view raw_line : split_lines(raw)) {
    const std::string trimmed = trim_ascii(raw_line);
    if (!trimmed.empty()) {
      lines.push_back(trimmed);
    }
  }

  std::string header_row;
  std::vector<std::string> data_lines;
  for (const std::string& line : lines) {
    if (line.starts_with('#') || line.starts_with('%')) {
      continue;
    }

    const char delimiter = line.find('\t') != std::string::npos ? '\t' : ',';
    const std::vector<std::string> parts = split_cells(line, delimiter);
    if (parts.size() >= 2 && !parse_double_full(parts[0])) {
      header_row = line;
      continue;
    }
    if (parts.size() >= 2) {
      data_lines.push_back(line);
    }
  }

  if (data_lines.empty()) {
    return make_error(KERNEL_SPECTROSCOPY_PARSE_ERROR_CSV_NO_NUMERIC_ROWS);
  }

  const char delimiter = data_lines[0].find('\t') != std::string::npos ? '\t' : ',';
  const std::vector<std::string> first_parts = split_cells(data_lines[0], delimiter);
  const std::size_t column_count = first_parts.size();
  if (column_count < 2) {
    return make_error(KERNEL_SPECTROSCOPY_PARSE_ERROR_CSV_TOO_FEW_COLUMNS);
  }

  SpectroscopyParseResult result{};
  result.data.x_label = "X";
  std::vector<std::vector<double>> columns(column_count - 1);

  for (const std::string& line : data_lines) {
    const std::vector<std::string> parts = split_cells(line, delimiter);
    double x_value = 0.0;
    if (!parse_double_full(parts.empty() ? std::string_view{} : std::string_view(parts[0]), x_value)) {
      continue;
    }

    result.data.x.push_back(x_value);
    for (std::size_t column = 1; column < column_count; ++column) {
      double y_value = 0.0;
      if (column < parts.size()) {
        const bool parsed = parse_double_full(parts[column], y_value);
        if (!parsed) {
          y_value = 0.0;
        }
      }
      columns[column - 1].push_back(y_value);
    }
  }

  if (result.data.x.empty()) {
    return make_error(KERNEL_SPECTROSCOPY_PARSE_ERROR_CSV_NO_VALID_POINTS);
  }

  std::vector<std::string> y_labels;
  if (!header_row.empty()) {
    const char header_delimiter =
        header_row.find('\t') != std::string::npos ? '\t' : ',';
    const std::vector<std::string> headers = split_cells(header_row, header_delimiter);
    if (headers.size() >= 2) {
      result.data.x_label = headers[0];
      for (std::size_t index = 1; index < headers.size(); ++index) {
        y_labels.push_back(headers[index]);
      }
    }
  }

  result.data.series.reserve(columns.size());
  for (std::size_t index = 0; index < columns.size(); ++index) {
    SpectroscopySeries series{};
    series.y = std::move(columns[index]);
    if (index < y_labels.size()) {
      series.label = y_labels[index];
    } else {
      series.label = "Series " + std::to_string(index + 1);
    }
    result.data.series.push_back(std::move(series));
  }

  double x_min = std::numeric_limits<double>::infinity();
  double x_max = -std::numeric_limits<double>::infinity();
  const std::size_t sample_count = std::min<std::size_t>(100, result.data.x.size());
  for (std::size_t index = 0; index < sample_count; ++index) {
    x_min = std::min(x_min, result.data.x[index]);
    x_max = std::max(x_max, result.data.x[index]);
  }
  result.data.is_nmr = x_min >= -2.0 && x_max <= 220.0 && (x_max - x_min) < 250.0;
  return result;
}

SpectroscopyParseResult parse_jdx_spectroscopy(std::string_view raw) {
  SpectroscopyParseResult result{};
  result.data.x_label = "X";
  result.data.title = "";
  std::string y_label = "Y";
  std::string data_type;
  bool in_data = false;

  for (const std::string_view raw_line : split_lines(raw)) {
    const std::string trimmed = trim_ascii(raw_line);
    if (trimmed.empty()) {
      continue;
    }

    if (trimmed.starts_with("##TITLE=")) {
      result.data.title = trim_ascii(std::string_view(trimmed).substr(8));
      continue;
    }
    if (trimmed.starts_with("##XUNITS=")) {
      result.data.x_label = trim_ascii(std::string_view(trimmed).substr(9));
      continue;
    }
    if (trimmed.starts_with("##YUNITS=")) {
      y_label = trim_ascii(std::string_view(trimmed).substr(9));
      continue;
    }
    if (trimmed.starts_with("##DATATYPE=")) {
      data_type = trim_ascii(std::string_view(trimmed).substr(11));
      continue;
    }
    if (trimmed.starts_with("##XYDATA=") || trimmed.starts_with("##PEAK TABLE=")) {
      in_data = true;
      continue;
    }
    if (trimmed.starts_with("##END=")) {
      break;
    }
    if (trimmed.starts_with("##")) {
      in_data = false;
      continue;
    }

    if (in_data) {
      for (const auto& [x_value, y_value] : parse_numeric_pairs(trimmed)) {
        result.data.x.push_back(x_value);
        if (result.data.series.empty()) {
          result.data.series.push_back(SpectroscopySeries{});
          result.data.series[0].label = y_label;
        }
        result.data.series[0].y.push_back(y_value);
      }
    }
  }

  if (result.data.x.empty()) {
    return make_error(KERNEL_SPECTROSCOPY_PARSE_ERROR_JDX_NO_POINTS);
  }

  if (result.data.series.empty()) {
    result.data.series.push_back(SpectroscopySeries{});
    result.data.series[0].label = y_label;
  }

  const std::string data_type_lower = to_lower_ascii(data_type);
  const std::string x_label_lower = to_lower_ascii(result.data.x_label);
  result.data.is_nmr =
      data_type_lower.find("nmr") != std::string::npos ||
      x_label_lower.find("ppm") != std::string::npos ||
      x_label_lower.find("chemical shift") != std::string::npos;
  return result;
}

}  // namespace

SpectroscopyParseResult parse_spectroscopy_text(
    std::string_view raw,
    std::string_view extension) {
  const std::string normalized_extension = to_lower_ascii(trim_ascii(extension));
  if (normalized_extension == "jdx") {
    return parse_jdx_spectroscopy(raw);
  }
  if (normalized_extension == "csv") {
    return parse_csv_spectroscopy(raw);
  }
  return make_error(KERNEL_SPECTROSCOPY_PARSE_ERROR_UNSUPPORTED_EXTENSION);
}

}  // namespace kernel::chemistry
