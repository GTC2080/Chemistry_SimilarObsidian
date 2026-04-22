// Reason: Share the small PDF fixture builders and anchor-query helpers across
// Track 3 and Track 4 suites so newer tests do not keep re-declaring them.

#include "api/kernel_api_pdf_test_helpers.h"

#include "support/test_support.h"

std::string make_text_pdf_bytes(std::string_view page_text) {
  std::string bytes = "%PDF-1.7\n1 0 obj\n<< /Type /Catalog >>\nendobj\n";
  bytes += "2 0 obj\n<< /Type /Page >>\n";
  if (!page_text.empty()) {
    bytes += "BT\n/F1 12 Tf\n(";
    bytes += page_text;
    bytes += ") Tj\nET\n";
  }
  bytes += "endobj\n%%EOF\n";
  return bytes;
}

std::string make_metadata_pdf_bytes(
    const int page_count,
    const bool has_outline,
    const bool include_text_layer,
    std::string_view title_clause) {
  std::string bytes = "%PDF-1.7\n1 0 obj\n<< /Type /Catalog ";
  if (has_outline) {
    bytes += "/Outlines 2 0 R ";
  }
  bytes += std::string(title_clause);
  bytes += ">>\nendobj\n";
  for (int page = 0; page < page_count; ++page) {
    bytes += std::to_string(page + 2);
    bytes += " 0 obj\n<< /Type /Page >>\nendobj\n";
  }
  if (include_text_layer) {
    bytes += "BT\n/F1 12 Tf\n(hello) Tj\nET\n";
  }
  bytes += "%%EOF\n";
  return bytes;
}

bool try_query_pdf_anchor_records(
    kernel_handle* handle,
    const char* attachment_rel_path,
    std::vector<kernel::storage::PdfAnchorRecord>& out_records) {
  return kernel::core::pdf_query::query_live_pdf_anchor_records(
             handle,
             attachment_rel_path,
             out_records)
             .code == KERNEL_OK;
}

std::vector<kernel::storage::PdfAnchorRecord> query_pdf_anchor_records(
    kernel_handle* handle,
    const char* attachment_rel_path) {
  std::vector<kernel::storage::PdfAnchorRecord> records;
  expect_ok(
      kernel::core::pdf_query::query_live_pdf_anchor_records(
          handle,
          attachment_rel_path,
          records));
  return records;
}
