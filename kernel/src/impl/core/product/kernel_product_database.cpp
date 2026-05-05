// Reason: Own product database shape normalization separately from JSON parsing and C ABI marshaling.

#include "core/kernel_product_database.h"

#include "core/kernel_product_json.h"
#include "core/kernel_shared.h"

#include <chrono>
#include <cctype>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using JsonValue = kernel::core::product::ProductJsonValue;

bool is_ascii_space(const char ch) {
  const auto byte = static_cast<unsigned char>(ch);
  return std::isspace(byte) != 0;
}

std::string_view trim_ascii(std::string_view value) {
  while (!value.empty() && is_ascii_space(value.front())) {
    value.remove_prefix(1);
  }
  while (!value.empty() && is_ascii_space(value.back())) {
    value.remove_suffix(1);
  }
  return value;
}

std::string json_string(std::string_view value) {
  return "\"" + kernel::core::json_escape(value) + "\"";
}

bool is_allowed_database_column_type(std::string_view column_type) {
  return (
      column_type == "text" || column_type == "number" ||
      column_type == "select" || column_type == "tags");
}

std::string gen_database_id(std::string_view prefix, std::size_t index) {
  const auto ticks = static_cast<unsigned long long>(
      std::chrono::system_clock::now().time_since_epoch().count());
  char suffix[64]{};
  std::snprintf(
      suffix,
      sizeof(suffix),
      "_%llx_%zx",
      ticks & 0xFFFF'FFFFull,
      index);
  std::string id(prefix);
  id.append(suffix);
  return id;
}

std::string json_cell_value_or_empty(const JsonValue* value) {
  if (value == nullptr) {
    return "\"\"";
  }
  return value->raw.empty() ? "null" : value->raw;
}

std::string string_field_or_default(
    const JsonValue* object,
    std::string_view key,
    std::string_view fallback) {
  const JsonValue* value = object == nullptr ? nullptr : object->get(key);
  if (value == nullptr || value->kind != JsonValue::Kind::String) {
    return std::string(fallback);
  }
  return std::string(trim_ascii(value->string_value));
}

struct NormalizedDatabaseColumn {
  std::string id;
  std::string name;
  std::string type;
};

std::vector<NormalizedDatabaseColumn> normalize_database_columns(const JsonValue& root) {
  std::vector<NormalizedDatabaseColumn> columns;
  const JsonValue* raw_columns = root.kind == JsonValue::Kind::Object ? root.get("columns") : nullptr;
  if (raw_columns != nullptr && raw_columns->kind == JsonValue::Kind::Array) {
    for (const JsonValue& raw_column : raw_columns->array_values) {
      if (raw_column.kind != JsonValue::Kind::Object || raw_column.get("name") == nullptr) {
        continue;
      }
      std::string name = string_field_or_default(&raw_column, "name", "Untitled");
      if (name.empty()) {
        name = "Untitled";
      }
      std::string id = string_field_or_default(&raw_column, "id", "");
      if (id.empty()) {
        id = gen_database_id("col", columns.size() + 1);
      }
      const JsonValue* type_value = raw_column.get("type");
      const std::string type = kernel::core::product::normalize_database_column_type(
          type_value != nullptr && type_value->kind == JsonValue::Kind::String
              ? std::string_view(type_value->string_value)
              : std::string_view());
      columns.push_back({std::move(id), std::move(name), type});
    }
  }

  if (!columns.empty()) {
    return columns;
  }

  columns.push_back({gen_database_id("col", 1), "Name", "text"});
  columns.push_back({gen_database_id("col", 2), "Tags", "tags"});
  columns.push_back({gen_database_id("col", 3), "Notes", "text"});
  return columns;
}

std::string build_normalized_database_json(const JsonValue& root) {
  const std::vector<NormalizedDatabaseColumn> columns = normalize_database_columns(root);

  std::string output = "{\"columns\":[";
  for (std::size_t index = 0; index < columns.size(); ++index) {
    if (index != 0) {
      output.push_back(',');
    }
    output += "{\"id\":" + json_string(columns[index].id) +
              ",\"name\":" + json_string(columns[index].name) +
              ",\"type\":" + json_string(columns[index].type) + "}";
  }
  output += "],\"rows\":[";

  const JsonValue* raw_rows = root.kind == JsonValue::Kind::Object ? root.get("rows") : nullptr;
  bool wrote_row = false;
  std::size_t generated_row_index = 1;
  if (raw_rows != nullptr && raw_rows->kind == JsonValue::Kind::Array) {
    for (const JsonValue& raw_row : raw_rows->array_values) {
      if (raw_row.kind != JsonValue::Kind::Object) {
        continue;
      }
      if (wrote_row) {
        output.push_back(',');
      }
      wrote_row = true;

      std::string row_id = string_field_or_default(&raw_row, "id", "");
      if (row_id.empty()) {
        row_id = gen_database_id("row", generated_row_index++);
      }
      output += "{\"id\":" + json_string(row_id) + ",\"cells\":{";
      const JsonValue* raw_cells = raw_row.get("cells");
      for (std::size_t col_index = 0; col_index < columns.size(); ++col_index) {
        if (col_index != 0) {
          output.push_back(',');
        }
        const JsonValue* cell = nullptr;
        if (raw_cells != nullptr && raw_cells->kind == JsonValue::Kind::Object) {
          cell = raw_cells->get(columns[col_index].id);
        }
        output += json_string(columns[col_index].id) + ":" + json_cell_value_or_empty(cell);
      }
      output += "}}";
    }
  }

  output += "]}";
  return output;
}

}  // namespace

namespace kernel::core::product {

std::string normalize_database_column_type(std::string_view column_type) {
  if (is_allowed_database_column_type(column_type)) {
    return std::string(column_type);
  }
  return "text";
}

bool normalize_database_json(std::string_view raw_json, std::string& out_json) {
  JsonValue root;
  if (!parse_product_json(raw_json, root)) {
    out_json.clear();
    return false;
  }

  out_json = build_normalized_database_json(root);
  return true;
}

}  // namespace kernel::core::product
