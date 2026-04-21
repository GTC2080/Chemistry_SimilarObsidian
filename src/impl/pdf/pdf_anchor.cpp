// Reason: This file implements the minimal Track 3 Batch 2 page-anchor model
// so serialization, page text basis, and validation stay out of storage and ABI code.

#include "pdf/pdf_anchor.h"

#include "vault/revision.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <string_view>

namespace kernel::pdf {
namespace {

bool is_ascii_space(const char ch) {
  return std::isspace(static_cast<unsigned char>(ch)) != 0;
}

bool is_token_boundary(const char ch) {
  return ch == '\0' || is_ascii_space(ch) || ch == '[' || ch == ']' || ch == '(' ||
         ch == ')' || ch == '<' || ch == '>' || ch == '{' || ch == '}' ||
         ch == '/' || ch == '%';
}

std::size_t skip_ascii_space(std::string_view bytes, std::size_t offset) {
  while (offset < bytes.size() && is_ascii_space(bytes[offset])) {
    ++offset;
  }
  return offset;
}

std::size_t find_token(
    std::string_view bytes,
    std::string_view token,
    std::size_t start_offset = 0) {
  std::size_t offset = start_offset;
  while (true) {
    offset = bytes.find(token, offset);
    if (offset == std::string_view::npos) {
      return offset;
    }

    const char before = offset == 0 ? '\0' : bytes[offset - 1];
    const std::size_t after_offset = offset + token.size();
    const char after = after_offset >= bytes.size() ? '\0' : bytes[after_offset];
    if (is_token_boundary(before) && is_token_boundary(after)) {
      return offset;
    }

    ++offset;
  }
}

std::vector<std::size_t> find_page_offsets(std::string_view bytes) {
  std::vector<std::size_t> offsets;
  std::size_t offset = 0;
  while (true) {
    offset = bytes.find("/Type", offset);
    if (offset == std::string_view::npos) {
      return offsets;
    }

    const std::size_t value_offset = skip_ascii_space(bytes, offset + 5);
    if (value_offset + 5 <= bytes.size() && bytes.substr(value_offset, 5) == "/Page") {
      const std::size_t suffix_offset = value_offset + 5;
      const char suffix = suffix_offset >= bytes.size() ? '\0' : bytes[suffix_offset];
      if (!std::isalpha(static_cast<unsigned char>(suffix))) {
        offsets.push_back(offset);
      }
    }

    offset = value_offset;
  }
}

bool append_literal_pdf_string(
    std::string_view bytes,
    std::size_t& index,
    std::string& out_text) {
  if (index >= bytes.size() || bytes[index] != '(') {
    return false;
  }

  ++index;
  int depth = 1;
  while (index < bytes.size()) {
    const char ch = bytes[index];
    if (ch == '\\') {
      if (index + 1 >= bytes.size()) {
        return false;
      }

      const char escaped = bytes[index + 1];
      if (escaped >= '0' && escaped <= '9') {
        return false;
      }
      out_text.push_back(escaped);
      index += 2;
      continue;
    }

    ++index;
    if (ch == '(') {
      ++depth;
      out_text.push_back(ch);
      continue;
    }
    if (ch == ')') {
      --depth;
      if (depth == 0) {
        return true;
      }
      if (depth < 0) {
        return false;
      }
      out_text.push_back(ch);
      continue;
    }

    if (static_cast<unsigned char>(ch) < 0x20 && ch != '\t' && ch != '\n' && ch != '\r') {
      return false;
    }
    out_text.push_back(ch);
  }

  return false;
}

std::string normalize_excerpt_basis(std::string_view value) {
  std::string normalized;
  normalized.reserve(value.size());
  bool previous_was_space = true;
  for (const char ch : value) {
    if (is_ascii_space(ch)) {
      if (!previous_was_space) {
        normalized.push_back(' ');
      }
      previous_was_space = true;
      continue;
    }

    normalized.push_back(ch);
    previous_was_space = false;
  }

  if (!normalized.empty() && normalized.back() == ' ') {
    normalized.pop_back();
  }
  return normalized;
}

std::string truncate_excerpt(std::string_view normalized_basis) {
  if (normalized_basis.size() <= kPdfAnchorExcerptMaxBytes) {
    return std::string(normalized_basis);
  }
  return std::string(normalized_basis.substr(0, kPdfAnchorExcerptMaxBytes));
}

std::string percent_encode(std::string_view value) {
  std::ostringstream output;
  output << std::uppercase << std::hex;
  for (const unsigned char ch : value) {
    const bool safe = std::isalnum(ch) != 0 || ch == '/' || ch == '.' || ch == '_' ||
                      ch == '-';
    if (safe) {
      output << static_cast<char>(ch);
      continue;
    }
    output << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(ch);
  }
  return output.str();
}

bool percent_decode(std::string_view value, std::string& out_value) {
  out_value.clear();
  out_value.reserve(value.size());
  for (std::size_t index = 0; index < value.size(); ++index) {
    if (value[index] != '%') {
      out_value.push_back(value[index]);
      continue;
    }
    if (index + 2 >= value.size()) {
      return false;
    }

    unsigned int decoded = 0;
    std::istringstream input(std::string(value.substr(index + 1, 2)));
    input >> std::hex >> decoded;
    if (!input || decoded > 0xffu) {
      return false;
    }
    out_value.push_back(static_cast<char>(decoded));
    index += 2;
  }
  return true;
}

std::string extract_page_text_basis(std::string_view page_segment) {
  std::string raw_text;
  std::size_t offset = 0;
  while (true) {
    const std::size_t begin_text = find_token(page_segment, "BT", offset);
    if (begin_text == std::string_view::npos) {
      return normalize_excerpt_basis(raw_text);
    }
    const std::size_t end_text = find_token(page_segment, "ET", begin_text + 2);
    if (end_text == std::string_view::npos) {
      return normalize_excerpt_basis(raw_text);
    }

    std::size_t cursor = begin_text + 2;
    while (cursor < end_text) {
      const std::size_t literal_offset = page_segment.find('(', cursor);
      if (literal_offset == std::string_view::npos || literal_offset >= end_text) {
        break;
      }

      std::string literal_text;
      std::size_t literal_cursor = literal_offset;
      if (append_literal_pdf_string(page_segment, literal_cursor, literal_text)) {
        if (!raw_text.empty() && !literal_text.empty()) {
          raw_text.push_back(' ');
        }
        raw_text.append(literal_text);
      }
      cursor = literal_cursor;
    }

    offset = end_text + 2;
  }
}

}  // namespace

std::string build_pdf_anchor_basis_revision(
    std::string_view attachment_content_revision,
    std::string_view anchor_relevant_text_basis,
    std::string_view anchor_mode) {
  if (attachment_content_revision.empty()) {
    return {};
  }

  return kernel::vault::compute_content_revision(
      std::string(attachment_content_revision) + "\n" + std::string(anchor_mode) + "\n" +
      std::string(anchor_relevant_text_basis));
}

std::string build_pdf_excerpt_fingerprint(std::string_view anchor_relevant_text_basis) {
  return kernel::vault::compute_content_revision(anchor_relevant_text_basis);
}

std::string serialize_pdf_anchor(const ParsedPdfAnchor& anchor) {
  return "pdfa:v1|path=" + percent_encode(anchor.rel_path) + "|basis=" +
         anchor.pdf_anchor_basis_revision + "|page=" + std::to_string(anchor.page) + "|xfp=" +
         anchor.excerpt_fingerprint;
}

bool parse_pdf_anchor(std::string_view serialized_anchor, ParsedPdfAnchor& out_anchor) {
  out_anchor = ParsedPdfAnchor{};
  constexpr std::string_view kPrefix = "pdfa:v1|path=";
  if (!serialized_anchor.starts_with(kPrefix)) {
    return false;
  }

  const std::size_t basis_pos = serialized_anchor.find("|basis=");
  const std::size_t page_pos = serialized_anchor.find("|page=");
  const std::size_t xfp_pos = serialized_anchor.find("|xfp=");
  if (basis_pos == std::string_view::npos || page_pos == std::string_view::npos ||
      xfp_pos == std::string_view::npos || !(basis_pos < page_pos && page_pos < xfp_pos)) {
    return false;
  }

  if (!percent_decode(
          serialized_anchor.substr(kPrefix.size(), basis_pos - kPrefix.size()),
          out_anchor.rel_path)) {
    return false;
  }
  out_anchor.pdf_anchor_basis_revision =
      std::string(serialized_anchor.substr(basis_pos + 7, page_pos - (basis_pos + 7)));
  out_anchor.excerpt_fingerprint =
      std::string(serialized_anchor.substr(xfp_pos + 5));
  const std::string page_text =
      std::string(serialized_anchor.substr(page_pos + 6, xfp_pos - (page_pos + 6)));
  const char* begin = page_text.data();
  const char* end = begin + page_text.size();
  const auto parse_result = std::from_chars(begin, end, out_anchor.page);
  return parse_result.ec == std::errc{} && parse_result.ptr == end && out_anchor.page != 0 &&
         !out_anchor.rel_path.empty() && !out_anchor.pdf_anchor_basis_revision.empty() &&
         !out_anchor.excerpt_fingerprint.empty();
}

std::vector<kernel::storage::PdfAnchorRecord> extract_pdf_anchor_records(
    std::string_view rel_path,
    std::string_view bytes,
    std::string_view attachment_content_revision) {
  (void)attachment_content_revision;
  std::vector<kernel::storage::PdfAnchorRecord> records;
  if (!bytes.starts_with("%PDF-")) {
    return records;
  }

  const std::vector<std::size_t> page_offsets = find_page_offsets(bytes);
  records.reserve(page_offsets.size());
  for (std::size_t index = 0; index < page_offsets.size(); ++index) {
    const std::size_t segment_start = page_offsets[index];
    const std::size_t segment_end =
        index + 1 < page_offsets.size() ? page_offsets[index + 1] : bytes.size();
    const std::string_view page_segment =
        bytes.substr(segment_start, segment_end - segment_start);
    const std::string excerpt_basis =
        extract_page_text_basis(page_segment);
    const std::string page_content_revision =
        kernel::vault::compute_content_revision(page_segment);

    kernel::storage::PdfAnchorRecord record{};
    record.rel_path = std::string(rel_path);
    record.page = index + 1;
    record.pdf_anchor_basis_revision =
        build_pdf_anchor_basis_revision(page_content_revision, excerpt_basis);
    record.excerpt_fingerprint = build_pdf_excerpt_fingerprint(excerpt_basis);
    record.excerpt_text = truncate_excerpt(excerpt_basis);
    record.anchor_serialized = serialize_pdf_anchor(
        ParsedPdfAnchor{
            std::string(rel_path),
            record.pdf_anchor_basis_revision,
            record.excerpt_fingerprint,
            record.page});
    records.push_back(std::move(record));
  }

  return records;
}

PdfAnchorValidationResult validate_pdf_anchor(
    std::string_view serialized_anchor,
    const kernel::storage::PdfMetadataRecord* current_metadata,
    const kernel::storage::PdfAnchorRecord* current_anchor) {
  PdfAnchorValidationResult result{};
  if (!parse_pdf_anchor(serialized_anchor, result.requested_anchor)) {
    result.state = PdfAnchorValidationState::Unverifiable;
    return result;
  }

  if (current_metadata != nullptr) {
    result.current_metadata = *current_metadata;
  }
  if (current_anchor != nullptr) {
    result.current_anchor = *current_anchor;
  }

  if (current_metadata == nullptr || current_metadata->rel_path.empty() ||
      current_metadata->is_missing) {
    result.state = PdfAnchorValidationState::Unavailable;
    return result;
  }

  if (current_metadata->metadata_state != kernel::storage::PdfMetadataState::Ready) {
    result.state = PdfAnchorValidationState::Unverifiable;
    return result;
  }

  if (current_anchor == nullptr || current_anchor->anchor_serialized.empty()) {
    result.state = PdfAnchorValidationState::Stale;
    return result;
  }

  result.state =
      current_anchor->anchor_serialized == serialized_anchor
          ? PdfAnchorValidationState::Resolved
          : PdfAnchorValidationState::Stale;
  return result;
}

}  // namespace kernel::pdf
