// Reason: Keep Track 3 Batch 1 PDF metadata surface coverage separate from
// attachment and diagnostics suites so future PDF anchor work has a clean home.

#include "kernel/c_api.h"

#include "api/kernel_api_pdf_surface_suites.h"
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

void require_pdf_not_found(
    kernel_handle* handle,
    const char* attachment_rel_path,
    std::string_view context) {
  kernel_pdf_metadata_record metadata{};
  const kernel_status status = kernel_get_pdf_metadata(handle, attachment_rel_path, &metadata);
  require_true(
      status.code == KERNEL_ERROR_NOT_FOUND,
      std::string(context) + ": pdf lookup should return NOT_FOUND");
  require_true(
      metadata.rel_path == nullptr && metadata.doc_title == nullptr &&
          metadata.pdf_metadata_revision == nullptr,
      std::string(context) + ": pdf lookup should clear stale output on NOT_FOUND");
}

void test_pdf_metadata_surface_reports_ready_partial_invalid_unavailable_and_not_found() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "assets");
  write_file_bytes(
      vault / "assets" / "ready.pdf",
      make_pdf_bytes(2, true, true, "/Title (Ready PDF) "));
  write_file_bytes(
      vault / "assets" / "partial.pdf",
      make_pdf_bytes(0, false, false, "/Title (Partial PDF) "));
  write_file_bytes(vault / "assets" / "invalid.pdf", "not-a-pdf");
  write_file_bytes(
      vault / "assets" / "title-unavailable.pdf",
      make_pdf_bytes(1, false, true, "/Title <FEFF005000440046> "));
  write_file_bytes(vault / "assets" / "plain.png", "png-bytes");
  write_file_bytes(
      vault / "assets" / "unreferenced.pdf",
      make_pdf_bytes(1, false, true, "/Title (Unreferenced PDF) "));

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string note =
      "# PDF Surface\n"
      "[Ready](assets/ready.pdf)\n"
      "[Partial](assets/partial.pdf)\n"
      "[Invalid](assets/invalid.pdf)\n"
      "[Missing](assets/missing.pdf)\n"
      "[TitleUnavailable](assets/title-unavailable.pdf)\n"
      "![Plain](assets/plain.png)\n";
  expect_ok(kernel_write_note(
      handle,
      "pdf-surface.md",
      note.data(),
      note.size(),
      nullptr,
      &metadata,
      &disposition));

  kernel_pdf_metadata_record pdf{};
  expect_ok(kernel_get_pdf_metadata(handle, "assets/ready.pdf", &pdf));
  require_true(
      std::string(pdf.rel_path) == "assets/ready.pdf",
      "pdf metadata surface should preserve the normalized rel_path");
  require_true(
      pdf.presence == KERNEL_ATTACHMENT_PRESENCE_PRESENT,
      "pdf metadata surface should preserve present state");
  require_true(
      pdf.metadata_state == KERNEL_PDF_METADATA_READY,
      "pdf metadata surface should expose READY state for valid extracted PDFs");
  require_true(
      pdf.page_count == 2,
      "pdf metadata surface should expose counted page objects");
  require_true(
      pdf.has_outline == 1,
      "pdf metadata surface should expose outline presence");
  require_true(
      pdf.doc_title_state == KERNEL_PDF_DOC_TITLE_AVAILABLE &&
          std::string(pdf.doc_title) == "Ready PDF",
      "pdf metadata surface should expose a stable literal doc title");
  require_true(
      pdf.text_layer_state == KERNEL_PDF_TEXT_LAYER_PRESENT,
      "pdf metadata surface should expose text-layer presence");
  require_true(
      pdf.pdf_metadata_revision != nullptr &&
          std::string(pdf.pdf_metadata_revision).find("track3_batch1_headless_minimal_parse_v1") !=
              std::string::npos,
      "pdf metadata surface should expose a metadata revision tied to the frozen extract mode");
  kernel_free_pdf_metadata_record(&pdf);

  expect_ok(kernel_get_pdf_metadata(handle, "assets/partial.pdf", &pdf));
  require_true(
      pdf.metadata_state == KERNEL_PDF_METADATA_PARTIAL && pdf.page_count == 0,
      "pdf metadata surface should expose PARTIAL when page count cannot be rebuilt");
  require_true(
      pdf.doc_title_state == KERNEL_PDF_DOC_TITLE_AVAILABLE &&
          std::string(pdf.doc_title) == "Partial PDF",
      "pdf metadata surface should still preserve stable optional fields for PARTIAL rows");
  kernel_free_pdf_metadata_record(&pdf);

  expect_ok(kernel_get_pdf_metadata(handle, "assets/invalid.pdf", &pdf));
  require_true(
      pdf.metadata_state == KERNEL_PDF_METADATA_INVALID,
      "pdf metadata surface should expose INVALID for malformed .pdf files");
  require_true(
      pdf.doc_title_state == KERNEL_PDF_DOC_TITLE_UNAVAILABLE && pdf.doc_title == nullptr,
      "pdf metadata surface should degrade doc_title to UNAVAILABLE for invalid PDFs");
  require_true(
      pdf.text_layer_state == KERNEL_PDF_TEXT_LAYER_UNAVAILABLE,
      "pdf metadata surface should degrade text-layer state to UNAVAILABLE for invalid PDFs");
  require_true(
      pdf.pdf_metadata_revision != nullptr,
      "invalid PDFs should still expose a metadata revision derived from attachment content");
  kernel_free_pdf_metadata_record(&pdf);

  expect_ok(kernel_get_pdf_metadata(handle, "assets/title-unavailable.pdf", &pdf));
  require_true(
      pdf.metadata_state == KERNEL_PDF_METADATA_READY,
      "pdf metadata surface should keep READY when core metadata is available");
  require_true(
      pdf.doc_title_state == KERNEL_PDF_DOC_TITLE_UNAVAILABLE && pdf.doc_title == nullptr,
      "pdf metadata surface should freeze unsupported title encodings as UNAVAILABLE");
  kernel_free_pdf_metadata_record(&pdf);

  expect_ok(kernel_get_pdf_metadata(handle, "assets/missing.pdf", &pdf));
  require_true(
      pdf.presence == KERNEL_ATTACHMENT_PRESENCE_MISSING,
      "pdf metadata surface should preserve live missing state");
  require_true(
      pdf.metadata_state == KERNEL_PDF_METADATA_UNAVAILABLE && pdf.page_count == 0,
      "pdf metadata surface should degrade never-extracted missing PDFs to UNAVAILABLE");
  require_true(
      pdf.pdf_metadata_revision == nullptr,
      "pdf metadata surface should leave metadata_revision empty when no extraction was possible");
  kernel_free_pdf_metadata_record(&pdf);

  require_pdf_not_found(handle, "assets/plain.png", "non-pdf live attachment");
  require_pdf_not_found(handle, "assets/unreferenced.pdf", "unreferenced disk pdf");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_pdf_metadata_surface_preserves_last_extracted_state_after_delete_and_reopen() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "assets");
  write_file_bytes(
      vault / "assets" / "carry.pdf",
      make_pdf_bytes(3, false, true, "/Title (Carry Forward PDF) "));

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string note =
      "# Carry Forward\n"
      "[Carry](assets/carry.pdf)\n";
  expect_ok(kernel_write_note(
      handle,
      "carry.md",
      note.data(),
      note.size(),
      nullptr,
      &metadata,
      &disposition));

  kernel_pdf_metadata_record before_delete{};
  expect_ok(kernel_get_pdf_metadata(handle, "assets/carry.pdf", &before_delete));
  const std::string before_revision = before_delete.pdf_metadata_revision;
  const std::string before_title = before_delete.doc_title;
  const std::uint64_t before_page_count = before_delete.page_count;
  kernel_free_pdf_metadata_record(&before_delete);

  std::filesystem::remove(vault / "assets" / "carry.pdf");
  require_eventually(
      [&]() {
        kernel_pdf_metadata_record after_delete{};
        const kernel_status status =
            kernel_get_pdf_metadata(handle, "assets/carry.pdf", &after_delete);
        if (status.code != KERNEL_OK) {
          return false;
        }

        const bool matches =
            after_delete.presence == KERNEL_ATTACHMENT_PRESENCE_MISSING &&
            after_delete.metadata_state == KERNEL_PDF_METADATA_READY &&
            after_delete.page_count == before_page_count &&
            after_delete.pdf_metadata_revision != nullptr &&
            std::string(after_delete.pdf_metadata_revision) == before_revision &&
            after_delete.doc_title != nullptr &&
            std::string(after_delete.doc_title) == before_title;
        kernel_free_pdf_metadata_record(&after_delete);
        return matches;
      },
      "pdf metadata surface should preserve last extracted metadata after watcher delete");

  expect_ok(kernel_close(handle));

  handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  kernel_pdf_metadata_record reopened{};
  expect_ok(kernel_get_pdf_metadata(handle, "assets/carry.pdf", &reopened));
  require_true(
      reopened.presence == KERNEL_ATTACHMENT_PRESENCE_MISSING,
      "pdf metadata surface should preserve missing state after reopen");
  require_true(
      reopened.metadata_state == KERNEL_PDF_METADATA_READY &&
          reopened.page_count == before_page_count,
      "pdf metadata surface should preserve extracted state after reopen");
  require_true(
      reopened.pdf_metadata_revision != nullptr &&
          std::string(reopened.pdf_metadata_revision) == before_revision,
      "pdf metadata surface should preserve revision after reopen");
  require_true(
      reopened.doc_title != nullptr && std::string(reopened.doc_title) == before_title,
      "pdf metadata surface should preserve doc title after reopen");
  kernel_free_pdf_metadata_record(&reopened);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_pdf_surface_metadata_tests() {
  test_pdf_metadata_surface_reports_ready_partial_invalid_unavailable_and_not_found();
  test_pdf_metadata_surface_preserves_last_extracted_state_after_delete_and_reopen();
}
