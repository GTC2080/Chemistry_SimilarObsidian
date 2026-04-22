// Reason: Keep PDF support-bundle coverage separate from attachment diagnostics
// so Track 3 can extend diagnostics without bloating existing suites.

#include "kernel/c_api.h"

#include "api/kernel_api_pdf_diagnostics_suites.h"
#include "core/kernel_pdf_query_shared.h"
#include "support/test_support.h"

#include <filesystem>
#include <string>
#include <vector>

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

std::string make_anchor_pdf_bytes(
    std::string_view title_clause,
    const std::vector<std::string>& page_texts,
    const bool has_outline) {
  std::string bytes = "%PDF-1.7\n1 0 obj\n<< /Type /Catalog ";
  if (has_outline) {
    bytes += "/Outlines 2 0 R ";
  }
  bytes += std::string(title_clause);
  bytes += ">>\nendobj\n";

  for (std::size_t page = 0; page < page_texts.size(); ++page) {
    bytes += std::to_string(page + 2);
    bytes += " 0 obj\n<< /Type /Page >>\n";
    if (!page_texts[page].empty()) {
      bytes += "BT\n/F1 12 Tf\n(";
      bytes += page_texts[page];
      bytes += ") Tj\nET\n";
    }
    bytes += "endobj\n";
  }

  bytes += "%%EOF\n";
  return bytes;
}

std::vector<kernel::storage::PdfAnchorRecord> query_pdf_anchors(
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

void test_pdf_diagnostics_snapshot_exports_contract_and_state_counts() {
  const auto vault = make_temp_vault();
  const auto export_path = vault / "pdf-diagnostics.json";
  std::filesystem::create_directories(vault / "assets");
  write_file_bytes(
      vault / "assets" / "ready.pdf",
      make_anchor_pdf_bytes(
          "/Title (Ready PDF) ",
          {"Ready Page One", "Ready Page Two"},
          true));
  write_file_bytes(
      vault / "assets" / "partial.pdf",
      make_pdf_bytes(0, false, false, "/Title (Partial PDF) "));
  write_file_bytes(vault / "assets" / "invalid.pdf", "not-a-pdf");
  write_file_bytes(
      vault / "assets" / "stale.pdf",
      make_anchor_pdf_bytes(
          "/Title (Stale PDF) ",
          {"Original Stale Text"},
          false));
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
      "[Stale](assets/stale.pdf)\n"
      "![Plain](assets/plain.png)\n";
  expect_ok(kernel_write_note(
      handle,
      "pdf-diagnostics.md",
      note.data(),
      note.size(),
      nullptr,
      &metadata,
      &disposition));

  const auto ready_anchor_records = query_pdf_anchors(handle, "assets/ready.pdf");
  const auto stale_anchor_records = query_pdf_anchors(handle, "assets/stale.pdf");
  require_true(
      ready_anchor_records.size() == 2 && stale_anchor_records.size() == 1,
      "pdf diagnostics snapshot test should materialize ready and stale anchor records before source refs");

  const std::string missing_anchor =
      "pdfa:v1|path=assets/missing.pdf|basis=missing-basis|page=1|xfp=missing-xfp";
  const std::string source_note =
      "# PDF Diagnostics Source Refs\n"
      "[Resolved](assets/ready.pdf#anchor=" + ready_anchor_records[0].anchor_serialized + ")\n"
      "[Missing](assets/missing.pdf#anchor=" + missing_anchor + ")\n"
      "[Stale](assets/stale.pdf#anchor=" + stale_anchor_records[0].anchor_serialized + ")\n"
      "[Broken](assets/ready.pdf#anchor=broken-anchor)\n";
  expect_ok(kernel_write_note(
      handle,
      "pdf-diagnostics-source.md",
      source_note.data(),
      source_note.size(),
      nullptr,
      &metadata,
      &disposition));

  write_file_bytes(
      vault / "assets" / "stale.pdf",
      make_anchor_pdf_bytes(
          "/Title (Stale PDF Updated) ",
          {"Changed Stale Text"},
          false));
  require_eventually(
      [&]() {
        kernel_pdf_source_refs refs{};
        const kernel_status status =
            kernel_query_note_pdf_source_refs(handle, "pdf-diagnostics-source.md", 8, &refs);
        if (status.code != KERNEL_OK) {
          kernel_free_pdf_source_refs(&refs);
          return false;
        }

        const bool matches =
            refs.count == 4 &&
            refs.refs[0].state == KERNEL_PDF_REF_RESOLVED &&
            refs.refs[1].state == KERNEL_PDF_REF_MISSING &&
            refs.refs[2].state == KERNEL_PDF_REF_STALE &&
            refs.refs[3].state == KERNEL_PDF_REF_UNRESOLVED;
        kernel_free_pdf_source_refs(&refs);
        return matches;
      },
      "pdf diagnostics snapshot test should settle source-ref states before export");

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
      exported.find("\"pdf_live_count\":5") != std::string::npos,
      "pdf diagnostics should count only live referenced PDF attachments");
  require_true(
      exported.find("\"pdf_metadata_ready_count\":2") != std::string::npos,
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
  require_true(
      exported.find("\"pdf_live_anchor_count\":3") != std::string::npos,
      "pdf diagnostics should count live pdf anchor rows");
  require_true(
      exported.find("\"pdf_source_ref_count\":4") != std::string::npos,
      "pdf diagnostics should count formal note->pdf source refs");
  require_true(
      exported.find("\"pdf_source_ref_resolved_count\":1") != std::string::npos,
      "pdf diagnostics should count resolved pdf source refs");
  require_true(
      exported.find("\"pdf_source_ref_missing_count\":1") != std::string::npos,
      "pdf diagnostics should count missing pdf source refs");
  require_true(
      exported.find("\"pdf_source_ref_stale_count\":1") != std::string::npos,
      "pdf diagnostics should count stale pdf source refs");
  require_true(
      exported.find("\"pdf_source_ref_unresolved_count\":1") != std::string::npos,
      "pdf diagnostics should count unresolved pdf source refs");
  require_true(
      exported.find("\"pdf_metadata_anomaly_summary\":\"partial_invalid_unavailable_pdf_metadata\"") !=
          std::string::npos,
      "pdf diagnostics should summarize metadata anomalies from exported counts");
  require_true(
      exported.find("\"pdf_reference_anomaly_summary\":\"missing_stale_unresolved_pdf_references\"") !=
          std::string::npos,
      "pdf diagnostics should summarize reference anomalies from exported counts");
  require_true(
      exported.find("\"last_pdf_recount_reason\":\"\"") != std::string::npos,
      "pdf diagnostics should leave last_pdf_recount_reason empty before rebuild or watcher full rescan");
  require_true(
      exported.find("\"last_pdf_recount_at_ns\":0") != std::string::npos,
      "pdf diagnostics should leave last_pdf_recount_at_ns zero before rebuild or watcher full rescan");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_pdf_diagnostics_snapshot_tests() {
  test_pdf_diagnostics_snapshot_exports_contract_and_state_counts();
}
