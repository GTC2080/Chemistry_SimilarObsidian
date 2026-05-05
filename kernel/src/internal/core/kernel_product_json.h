// Reason: Share the small product JSON parser without embedding it in one feature file.

#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace kernel::core::product {

struct ProductJsonValue {
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
  std::vector<ProductJsonValue> array_values;
  std::vector<std::pair<std::string, ProductJsonValue>> object_values;

  const ProductJsonValue* get(std::string_view key) const;
};

bool parse_product_json(std::string_view input, ProductJsonValue& out_value);

}  // namespace kernel::core::product
