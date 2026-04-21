// Reason: Keep PDF support-bundle coverage separate from attachment diagnostics
// so Track 3 can extend diagnostics without bloating existing suites.

#include "kernel/c_api.h"

#include "api/kernel_api_pdf_diagnostics_suites.h"
#include "support/test_support.h"

#include <filesystem>
#include <string>

namespace {

std::string make_pdf_bytes(
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

void test_pdf_diagnostics_snapshot_exports_contract_and_state_counts() {
  const auto vault = make_temp_vault();
  const auto export_path = vault / "pdf-diagnostics.json";
  std::filesystem::create_directories(vault / "assets");
  write_file_bytes(
      vault / "assets" / "ready.pdf",
      make_pdf_bytes(2, true, true, "/Title (Ready PDF) "));
  write_file_bytes(
      vault / "assets" / "partial.pdf",
      make_pdf_bytes(0, false, false, "/Title (Partial PDF) "));
  write_file_bytes(vault / "assets" / "invalid.pdf", "not-a-pdf");
  write_file_bytes(vault / "assets" / "plain.png", "png-bytes");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string note =
      "# PDF Diagnostics\n"
      "[Ready](assets/ready.pdf)\n"
      "[Partial](assets/partial.pdf)\n"
      "[Invalid](assets/invalid.pdf)\n"
      "[Missing](assets/missing.pdf)\n"
      "![Plain](assets/plain.png)\n";
  expect_ok(kernel_write_note(
      handle,
      "pdf-diagnostics.md",
      note.data(),
      note.size(),
      nullptr,
      &metadata,
      &disposition));

  expect_ok(kernel_export_diagnostics(handle, export_path.string().c_str()));
  const std::string exported = read_file_text(export_path);

  require_true(
      exported.find("\"pdf_contract_revision\":\"track3_batch1_pdf_metadata_v1\"") !=
          std::string::npos,
      "pdf diagnostics should expose the current PDF contract revision");
  require_true(
      exported.find("\"pdf_extract_mode\":\"track3_batch1_headless_minimal_parse_v1\"") !=
          std::string::npos,
      "pdf diagnostics should expose the current PDF extract mode");
  require_true(
      exported.find("\"pdf_lookup_key_mode\":\"normalized_live_attachment_rel_path\"") !=
          std::string::npos,
      "pdf diagnostics should expose the frozen PDF lookup key mode");
  require_true(
      exported.find("\"pdf_anchor_mode\":\"track3_batch2_page_excerpt_v1\"") !=
          std::string::npos,
      "pdf diagnostics should expose the current PDF anchor mode");
  require_true(
      exported.find("\"pdf_live_count\":4") != std::string::npos,
      "pdf diagnostics should count only live referenced PDF attachments");
  require_true(
      exported.find("\"pdf_metadata_ready_count\":1") != std::string::npos,
      "pdf diagnostics should count ready PDF rows");
  require_true(
      exported.find("\"pdf_metadata_partial_count\":1") != std::string::npos,
      "pdf diagnostics should count partial PDF rows");
  require_true(
      exported.find("\"pdf_metadata_invalid_count\":1") != std::string::npos,
      "pdf diagnostics should count invalid PDF rows");
  require_true(
      exported.find("\"pdf_metadata_unavailable_count\":1") != std::string::npos,
      "pdf diagnostics should count unavailable live PDFs");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_pdf_diagnostics_snapshot_tests() {
  test_pdf_diagnostics_snapshot_exports_contract_and_state_counts();
}
