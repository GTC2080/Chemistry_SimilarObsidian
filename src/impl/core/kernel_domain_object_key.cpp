// Reason: This file owns the canonical Track 4 domain-object-key grammar so
// every domain-facing public surface shares one stable serialized identity.

#include "core/kernel_domain_object_key.h"

#include <array>
#include <cctype>
#include <charconv>
#include <string>
#include <string_view>

namespace {

constexpr std::string_view kPrefix = "dom:v1/";

constexpr std::array<char, 16> kHexDigits{
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

std::string_view carrier_kind_token(const kernel_domain_carrier_kind carrier_kind) {
  switch (carrier_kind) {
    case KERNEL_DOMAIN_CARRIER_PDF:
      return "pdf";
    case KERNEL_DOMAIN_CARRIER_ATTACHMENT:
    default:
      return "attachment";
  }
}

bool parse_carrier_kind_token(
    std::string_view token,
    kernel_domain_carrier_kind& out_carrier_kind) {
  if (token == "attachment") {
    out_carrier_kind = KERNEL_DOMAIN_CARRIER_ATTACHMENT;
    return true;
  }
  if (token == "pdf") {
    out_carrier_kind = KERNEL_DOMAIN_CARRIER_PDF;
    return true;
  }
  return false;
}

bool is_unreserved(const unsigned char ch) {
  return std::isalnum(ch) != 0 || ch == '-' || ch == '.' || ch == '_' || ch == '~';
}

std::string percent_encode(std::string_view value) {
  std::string encoded;
  encoded.reserve(value.size() * 3);
  for (const unsigned char ch : value) {
    if (is_unreserved(ch)) {
      encoded.push_back(static_cast<char>(ch));
      continue;
    }
    encoded.push_back('%');
    encoded.push_back(kHexDigits[(ch >> 4) & 0x0f]);
    encoded.push_back(kHexDigits[ch & 0x0f]);
  }
  return encoded;
}

bool decode_hex_pair(std::string_view pair, unsigned char& out_value) {
  unsigned int value = 0;
  const auto* begin = pair.data();
  const auto* end = pair.data() + pair.size();
  const auto result = std::from_chars(begin, end, value, 16);
  if (result.ec != std::errc{} || result.ptr != end || value > 0xffu) {
    return false;
  }
  out_value = static_cast<unsigned char>(value);
  return true;
}

bool percent_decode(std::string_view value, std::string& out_decoded) {
  out_decoded.clear();
  out_decoded.reserve(value.size());
  for (size_t index = 0; index < value.size(); ++index) {
    if (value[index] != '%') {
      out_decoded.push_back(value[index]);
      continue;
    }
    if (index + 2 >= value.size()) {
      return false;
    }
    unsigned char decoded = 0;
    if (!decode_hex_pair(value.substr(index + 1, 2), decoded)) {
      return false;
    }
    out_decoded.push_back(static_cast<char>(decoded));
    index += 2;
  }
  return true;
}

bool is_lower_ascii_token(std::string_view value) {
  if (value.empty()) {
    return false;
  }
  for (const unsigned char ch : value) {
    if (!(std::isdigit(ch) != 0 || (ch >= 'a' && ch <= 'z') || ch == '_' || ch == '.')) {
      return false;
    }
  }
  return true;
}

bool split_four_segments(
    std::string_view serialized_key,
    std::string_view& out_carrier_kind,
    std::string_view& out_encoded_carrier_key,
    std::string_view& out_subtype_namespace,
    std::string_view& out_subtype_name) {
  const std::string_view without_prefix = serialized_key.substr(kPrefix.size());
  const size_t first_sep = without_prefix.find('/');
  if (first_sep == std::string_view::npos) {
    return false;
  }
  const size_t second_sep = without_prefix.find('/', first_sep + 1);
  if (second_sep == std::string_view::npos) {
    return false;
  }
  const size_t third_sep = without_prefix.find('/', second_sep + 1);
  if (third_sep == std::string_view::npos) {
    return false;
  }
  if (without_prefix.find('/', third_sep + 1) != std::string_view::npos) {
    return false;
  }

  out_carrier_kind = without_prefix.substr(0, first_sep);
  out_encoded_carrier_key = without_prefix.substr(first_sep + 1, second_sep - first_sep - 1);
  out_subtype_namespace =
      without_prefix.substr(second_sep + 1, third_sep - second_sep - 1);
  out_subtype_name = without_prefix.substr(third_sep + 1);
  return !(out_carrier_kind.empty() || out_encoded_carrier_key.empty() ||
           out_subtype_namespace.empty() || out_subtype_name.empty());
}

}  // namespace

namespace kernel::core::domain_object_key {

std::string make_domain_object_key(
    const kernel_domain_carrier_kind carrier_kind,
    std::string_view carrier_key,
    std::string_view subtype_namespace,
    std::string_view subtype_name) {
  std::string serialized;
  serialized.reserve(
      kPrefix.size() + carrier_key.size() * 3 + subtype_namespace.size() +
      subtype_name.size() + 16);
  serialized.append(kPrefix);
  serialized.append(carrier_kind_token(carrier_kind));
  serialized.push_back('/');
  serialized.append(percent_encode(carrier_key));
  serialized.push_back('/');
  serialized.append(subtype_namespace);
  serialized.push_back('/');
  serialized.append(subtype_name);
  return serialized;
}

bool parse_domain_object_key(
    std::string_view serialized_key,
    ParsedDomainObjectKey& out_key) {
  if (!serialized_key.starts_with(kPrefix)) {
    return false;
  }

  std::string_view carrier_kind_segment;
  std::string_view encoded_carrier_key_segment;
  std::string_view subtype_namespace_segment;
  std::string_view subtype_name_segment;
  if (!split_four_segments(
          serialized_key,
          carrier_kind_segment,
          encoded_carrier_key_segment,
          subtype_namespace_segment,
          subtype_name_segment)) {
    return false;
  }

  kernel_domain_carrier_kind carrier_kind = KERNEL_DOMAIN_CARRIER_ATTACHMENT;
  if (!parse_carrier_kind_token(carrier_kind_segment, carrier_kind) ||
      !is_lower_ascii_token(subtype_namespace_segment) ||
      !is_lower_ascii_token(subtype_name_segment)) {
    return false;
  }

  std::string decoded_carrier_key;
  if (!percent_decode(encoded_carrier_key_segment, decoded_carrier_key)) {
    return false;
  }

  out_key.carrier_kind = carrier_kind;
  out_key.carrier_kind_token.assign(carrier_kind_segment);
  out_key.carrier_key = std::move(decoded_carrier_key);
  out_key.subtype_namespace.assign(subtype_namespace_segment);
  out_key.subtype_name.assign(subtype_name_segment);
  return true;
}

}  // namespace kernel::core::domain_object_key
