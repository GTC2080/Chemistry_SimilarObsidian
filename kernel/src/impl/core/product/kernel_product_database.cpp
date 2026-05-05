// Reason: Own product database JSON normalization so the public ABI file does
// not carry a parser and shape-normalization implementation inline.

#include "core/kernel_product_database.h"

#include "core/kernel_shared.h"

#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

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

struct JsonValue {
  enum class Kind {
    Null,
    Bool,
    Number,
    String,
    Array,
    Object,
  };

  Kind kind = Kind::Null;
  std::string raw;
  std::string string_value;
  std::vector<JsonValue> array_values;
  std::vector<std::pair<std::string, JsonValue>> object_values;

  const JsonValue* get(std::string_view key) const {
    if (kind != Kind::Object) {
      return nullptr;
    }
    for (const auto& entry : object_values) {
      if (entry.first == key) {
        return &entry.second;
      }
    }
    return nullptr;
  }
};

int hex_digit_value(const char ch) {
  if (ch >= '0' && ch <= '9') {
    return ch - '0';
  }
  if (ch >= 'a' && ch <= 'f') {
    return 10 + (ch - 'a');
  }
  if (ch >= 'A' && ch <= 'F') {
    return 10 + (ch - 'A');
  }
  return -1;
}

void append_utf8_codepoint(const std::uint32_t codepoint, std::string& out) {
  if (codepoint <= 0x7FU) {
    out.push_back(static_cast<char>(codepoint));
  } else if (codepoint <= 0x7FFU) {
    out.push_back(static_cast<char>(0xC0U | (codepoint >> 6U)));
    out.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
  } else if (codepoint <= 0xFFFFU) {
    out.push_back(static_cast<char>(0xE0U | (codepoint >> 12U)));
    out.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU)));
    out.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
  } else {
    out.push_back(static_cast<char>(0xF0U | (codepoint >> 18U)));
    out.push_back(static_cast<char>(0x80U | ((codepoint >> 12U) & 0x3FU)));
    out.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU)));
    out.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
  }
}

class JsonParser {
 public:
  explicit JsonParser(std::string_view input) : input_(input) {}

  bool parse(JsonValue& out_value) {
    skip_ws();
    if (!parse_value(out_value)) {
      return false;
    }
    skip_ws();
    return cursor_ == input_.size();
  }

 private:
  void skip_ws() {
    while (cursor_ < input_.size() && is_ascii_space(input_[cursor_])) {
      ++cursor_;
    }
  }

  bool consume(std::string_view token) {
    if (input_.substr(cursor_, token.size()) != token) {
      return false;
    }
    cursor_ += token.size();
    return true;
  }

  bool parse_hex4(std::uint32_t& out_value) {
    if (cursor_ + 4 > input_.size()) {
      return false;
    }
    std::uint32_t value = 0;
    for (std::size_t offset = 0; offset < 4; ++offset) {
      const int digit = hex_digit_value(input_[cursor_ + offset]);
      if (digit < 0) {
        return false;
      }
      value = (value << 4U) | static_cast<std::uint32_t>(digit);
    }
    cursor_ += 4;
    out_value = value;
    return true;
  }

  bool parse_string_contents(std::string& out_value) {
    if (cursor_ >= input_.size() || input_[cursor_] != '"') {
      return false;
    }
    ++cursor_;
    while (cursor_ < input_.size()) {
      const char ch = input_[cursor_++];
      if (ch == '"') {
        return true;
      }
      if (static_cast<unsigned char>(ch) < 0x20U) {
        return false;
      }
      if (ch != '\\') {
        out_value.push_back(ch);
        continue;
      }
      if (cursor_ >= input_.size()) {
        return false;
      }
      const char escaped = input_[cursor_++];
      switch (escaped) {
        case '"':
        case '\\':
        case '/':
          out_value.push_back(escaped);
          break;
        case 'b':
          out_value.push_back('\b');
          break;
        case 'f':
          out_value.push_back('\f');
          break;
        case 'n':
          out_value.push_back('\n');
          break;
        case 'r':
          out_value.push_back('\r');
          break;
        case 't':
          out_value.push_back('\t');
          break;
        case 'u': {
          std::uint32_t codepoint = 0;
          if (!parse_hex4(codepoint)) {
            return false;
          }
          append_utf8_codepoint(codepoint, out_value);
          break;
        }
        default:
          return false;
      }
    }
    return false;
  }

  bool parse_string(JsonValue& out_value) {
    const std::size_t start = cursor_;
    std::string decoded;
    if (!parse_string_contents(decoded)) {
      return false;
    }
    out_value.kind = JsonValue::Kind::String;
    out_value.string_value = std::move(decoded);
    out_value.raw = std::string(input_.substr(start, cursor_ - start));
    return true;
  }

  bool parse_number(JsonValue& out_value) {
    const std::size_t start = cursor_;
    if (cursor_ < input_.size() && input_[cursor_] == '-') {
      ++cursor_;
    }
    if (cursor_ >= input_.size()) {
      return false;
    }
    if (input_[cursor_] == '0') {
      ++cursor_;
    } else if (input_[cursor_] >= '1' && input_[cursor_] <= '9') {
      while (cursor_ < input_.size() && input_[cursor_] >= '0' && input_[cursor_] <= '9') {
        ++cursor_;
      }
    } else {
      return false;
    }
    if (cursor_ < input_.size() && input_[cursor_] == '.') {
      ++cursor_;
      const std::size_t fraction_start = cursor_;
      while (cursor_ < input_.size() && input_[cursor_] >= '0' && input_[cursor_] <= '9') {
        ++cursor_;
      }
      if (cursor_ == fraction_start) {
        return false;
      }
    }
    if (cursor_ < input_.size() && (input_[cursor_] == 'e' || input_[cursor_] == 'E')) {
      ++cursor_;
      if (cursor_ < input_.size() && (input_[cursor_] == '+' || input_[cursor_] == '-')) {
        ++cursor_;
      }
      const std::size_t exponent_start = cursor_;
      while (cursor_ < input_.size() && input_[cursor_] >= '0' && input_[cursor_] <= '9') {
        ++cursor_;
      }
      if (cursor_ == exponent_start) {
        return false;
      }
    }
    out_value.kind = JsonValue::Kind::Number;
    out_value.raw = std::string(input_.substr(start, cursor_ - start));
    return true;
  }

  bool parse_array(JsonValue& out_value) {
    const std::size_t start = cursor_;
    if (cursor_ >= input_.size() || input_[cursor_] != '[') {
      return false;
    }
    ++cursor_;
    skip_ws();
    out_value.kind = JsonValue::Kind::Array;
    if (cursor_ < input_.size() && input_[cursor_] == ']') {
      ++cursor_;
      out_value.raw = std::string(input_.substr(start, cursor_ - start));
      return true;
    }
    while (cursor_ < input_.size()) {
      JsonValue item;
      if (!parse_value(item)) {
        return false;
      }
      out_value.array_values.push_back(std::move(item));
      skip_ws();
      if (cursor_ < input_.size() && input_[cursor_] == ']') {
        ++cursor_;
        out_value.raw = std::string(input_.substr(start, cursor_ - start));
        return true;
      }
      if (cursor_ >= input_.size() || input_[cursor_] != ',') {
        return false;
      }
      ++cursor_;
      skip_ws();
    }
    return false;
  }

  bool parse_object(JsonValue& out_value) {
    const std::size_t start = cursor_;
    if (cursor_ >= input_.size() || input_[cursor_] != '{') {
      return false;
    }
    ++cursor_;
    skip_ws();
    out_value.kind = JsonValue::Kind::Object;
    if (cursor_ < input_.size() && input_[cursor_] == '}') {
      ++cursor_;
      out_value.raw = std::string(input_.substr(start, cursor_ - start));
      return true;
    }
    while (cursor_ < input_.size()) {
      std::string key;
      if (!parse_string_contents(key)) {
        return false;
      }
      skip_ws();
      if (cursor_ >= input_.size() || input_[cursor_] != ':') {
        return false;
      }
      ++cursor_;
      JsonValue value;
      if (!parse_value(value)) {
        return false;
      }
      out_value.object_values.emplace_back(std::move(key), std::move(value));
      skip_ws();
      if (cursor_ < input_.size() && input_[cursor_] == '}') {
        ++cursor_;
        out_value.raw = std::string(input_.substr(start, cursor_ - start));
        return true;
      }
      if (cursor_ >= input_.size() || input_[cursor_] != ',') {
        return false;
      }
      ++cursor_;
      skip_ws();
    }
    return false;
  }

  bool parse_literal(JsonValue& out_value, std::string_view token, JsonValue::Kind kind) {
    const std::size_t start = cursor_;
    if (!consume(token)) {
      return false;
    }
    out_value.kind = kind;
    out_value.raw = std::string(input_.substr(start, cursor_ - start));
    return true;
  }

  bool parse_value(JsonValue& out_value) {
    skip_ws();
    if (cursor_ >= input_.size()) {
      return false;
    }
    switch (input_[cursor_]) {
      case '"':
        return parse_string(out_value);
      case '{':
        return parse_object(out_value);
      case '[':
        return parse_array(out_value);
      case 't':
        return parse_literal(out_value, "true", JsonValue::Kind::Bool);
      case 'f':
        return parse_literal(out_value, "false", JsonValue::Kind::Bool);
      case 'n':
        return parse_literal(out_value, "null", JsonValue::Kind::Null);
      default:
        return parse_number(out_value);
    }
  }

  std::string_view input_;
  std::size_t cursor_ = 0;
};

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
  JsonParser parser(raw_json);
  if (!parser.parse(root)) {
    out_json.clear();
    return false;
  }

  out_json = build_normalized_database_json(root);
  return true;
}

}  // namespace kernel::core::product
