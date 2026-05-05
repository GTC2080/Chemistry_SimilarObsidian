// Reason: Own the small product JSON parser separately from feature normalization rules.

#include "core/kernel_product_json.h"

#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace {

bool is_ascii_space(const char ch) {
  const auto byte = static_cast<unsigned char>(ch);
  return std::isspace(byte) != 0;
}

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

class ProductJsonParser {
 public:
  explicit ProductJsonParser(std::string_view input) : input_(input) {}

  bool parse(kernel::core::product::ProductJsonValue& out_value) {
    skip_ws();
    if (!parse_value(out_value)) {
      return false;
    }
    skip_ws();
    return cursor_ == input_.size();
  }

 private:
  using JsonValue = kernel::core::product::ProductJsonValue;

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

}  // namespace

namespace kernel::core::product {

const ProductJsonValue* ProductJsonValue::get(std::string_view key) const {
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

bool parse_product_json(std::string_view input, ProductJsonValue& out_value) {
  ProductJsonParser parser(input);
  return parser.parse(out_value);
}

}  // namespace kernel::core::product
