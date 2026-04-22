// Reason: This file implements the minimal Track 3 headless PDF metadata
// extractor so PDF parsing details stay out of refresh and ABI code.

#include "pdf/pdf_metadata.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

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

std::string to_lower_ascii(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
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

std::uint64_t count_page_objects(std::string_view bytes) {
  std::uint64_t count = 0;
  std::size_t offset = 0;
  while (true) {
    offset = bytes.find("/Type", offset);
    if (offset == std::string_view::npos) {
      return count;
    }

    std::size_t value_offset = skip_ascii_space(bytes, offset + 5);
    if (value_offset + 5 <= bytes.size() && bytes.substr(value_offset, 5) == "/Page") {
      const std::size_t suffix_offset = value_offset + 5;
      const char suffix = suffix_offset >= bytes.size() ? '\0' : bytes[suffix_offset];
      if (!std::isalpha(static_cast<unsigned char>(suffix))) {
        ++count;
      }
    }

    offset = value_offset;
  }
}

bool detect_outline(std::string_view bytes) {
  return bytes.find("/Outlines") != std::string_view::npos;
}

kernel::storage::PdfTextLayerState detect_text_layer_state(std::string_view bytes) {
  const std::size_t begin_text = find_token(bytes, "BT");
  if (begin_text == std::string_view::npos) {
    return kernel::storage::PdfTextLayerState::Absent;
  }
  const std::size_t end_text = find_token(bytes, "ET", begin_text + 2);
  return end_text == std::string_view::npos
             ? kernel::storage::PdfTextLayerState::Absent
             : kernel::storage::PdfTextLayerState::Present;
}

struct DocTitleParseResult {
  kernel::storage::PdfDocTitleState state = kernel::storage::PdfDocTitleState::Unavailable;
  std::string value;
};

DocTitleParseResult parse_doc_title(std::string_view bytes) {
  const std::size_t title_offset = bytes.find("/Title");
  if (title_offset == std::string_view::npos) {
    return {kernel::storage::PdfDocTitleState::Absent, ""};
  }

  std::size_t value_offset = skip_ascii_space(bytes, title_offset + 6);
  if (value_offset >= bytes.size()) {
    return {};
  }
  if (bytes[value_offset] != '(') {
    return {};
  }

  std::string value;
  int depth = 0;
  for (std::size_t index = value_offset; index < bytes.size(); ++index) {
    const char ch = bytes[index];
    if (ch == '\\') {
      if (index + 1 >= bytes.size()) {
        return {};
      }

      const char escaped = bytes[index + 1];
      if (escaped >= '0' && escaped <= '9') {
        return {};
      }
      if (depth > 0) {
        value.push_back(escaped);
      }
      ++index;
      continue;
    }

    if (ch == '(') {
      ++depth;
      if (depth > 1) {
        value.push_back(ch);
      }
      continue;
    }

    if (ch == ')') {
      --depth;
      if (depth == 0) {
        return {kernel::storage::PdfDocTitleState::Available, value};
      }
      if (depth < 0) {
        return {};
      }
      value.push_back(ch);
      continue;
    }

    if (depth > 0) {
      if (static_cast<unsigned char>(ch) < 0x20 && ch != '\t' && ch != '\n' && ch != '\r') {
        return {};
      }
      value.push_back(ch);
    }
  }

  return {};
}

}  // namespace

bool is_pdf_rel_path(std::string_view rel_path) {
  const std::filesystem::path path{std::string(rel_path)};
  return to_lower_ascii(path.extension().generic_string()) == ".pdf";
}

std::string build_pdf_metadata_revision(std::string_view attachment_content_revision) {
  if (attachment_content_revision.empty()) {
    return {};
  }

  return std::string("pdfmeta:v1:") + std::string(attachment_content_revision) + ":" +
         std::string(kPdfExtractMode);
}

kernel::storage::PdfMetadataRecord extract_pdf_metadata(
    std::string_view rel_path,
    std::string_view bytes,
    std::string_view attachment_content_revision) {
  kernel::storage::PdfMetadataRecord record{};
  record.rel_path = std::string(rel_path);
  record.attachment_content_revision = std::string(attachment_content_revision);
  record.pdf_metadata_revision = build_pdf_metadata_revision(attachment_content_revision);

  if (!bytes.starts_with("%PDF-")) {
    record.metadata_state = kernel::storage::PdfMetadataState::Invalid;
    return record;
  }

  record.page_count = count_page_objects(bytes);
  record.has_outline = detect_outline(bytes);
  record.text_layer_state = detect_text_layer_state(bytes);

  const DocTitleParseResult title = parse_doc_title(bytes);
  record.doc_title_state = title.state;
  record.doc_title = title.value;

  record.metadata_state =
      record.page_count == 0 ? kernel::storage::PdfMetadataState::Partial
                             : kernel::storage::PdfMetadataState::Ready;
  return record;
}

}  // namespace kernel::pdf
