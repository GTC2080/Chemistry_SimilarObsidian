// Reason: Keep Track 3 Batch 2 PDF anchor substrate coverage separate from
// metadata-only tests so future note↔PDF reference suites have a clean boundary.

#include "api/kernel_api_pdf_surface_suites.h"

#include "core/kernel_pdf_query_shared.h"
#include "kernel/c_api.h"
#include "pdf/pdf_anchor.h"
#include "support/test_support.h"

#include <filesystem>
#include <string>
#include <vector>

namespace {

std::string make_pdf_bytes(
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
  const kernel_status status =
      kernel::core::pdf_query::query_live_pdf_anchor_records(
          handle,
          attachment_rel_path,
          records);
  expect_ok(status);
  return records;
}

bool try_query_pdf_anchors(
    kernel_handle* handle,
    const char* attachment_rel_path,
    std::vector<kernel::storage::PdfAnchorRecord>& out_records) {
  out_records.clear();
  const kernel_status status =
      kernel::core::pdf_query::query_live_pdf_anchor_records(
          handle,
          attachment_rel_path,
          out_records);
  return status.code == KERNEL_OK;
}

void test_pdf_anchor_serialization_roundtrips_and_ignores_unrelated_metadata_changes() {
  require_true(
      kernel::pdf::build_pdf_anchor_basis_revision("content-rev", "same-basis", "mode-a") !=
          kernel::pdf::build_pdf_anchor_basis_revision("content-rev", "same-basis", "mode-b"),
      "pdf anchor basis revision should change when anchor mode revision changes");

  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "assets");
  const std::string initial_bytes = make_pdf_bytes(
      "/Title (Anchor Doc) ",
      {"Page One Alpha", "Page Two Beta"},
      true);
  write_file_bytes(vault / "assets" / "anchor.pdf", initial_bytes);

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string note =
      "# PDF Anchor Surface\n"
      "[Anchor](assets/anchor.pdf)\n";
  expect_ok(kernel_write_note(
      handle,
      "anchor.md",
      note.data(),
      note.size(),
      nullptr,
      &metadata,
      &disposition));

  const auto initial_records = query_pdf_anchors(handle, "assets/anchor.pdf");
  require_true(
      initial_records.size() == 2,
      "pdf anchor substrate should materialize one page-level anchor per page");
  require_true(
      initial_records[0].page == 1 && initial_records[1].page == 2,
      "pdf anchor substrate should sort anchors by page");
  require_true(
      initial_records[0].excerpt_text == "Page One Alpha" &&
          initial_records[1].excerpt_text == "Page Two Beta",
      "pdf anchor substrate should preserve normalized page excerpt previews");

  kernel::pdf::ParsedPdfAnchor parsed_anchor;
  require_true(
      kernel::pdf::parse_pdf_anchor(initial_records[0].anchor_serialized, parsed_anchor),
      "pdf anchor substrate should parse canonical serialized anchors");
  require_true(
      kernel::pdf::serialize_pdf_anchor(parsed_anchor) ==
          initial_records[0].anchor_serialized,
      "pdf anchor serialization should roundtrip without drift");
  require_true(
      parsed_anchor.rel_path == "assets/anchor.pdf" &&
          parsed_anchor.page == 1 &&
          parsed_anchor.pdf_anchor_basis_revision ==
              initial_records[0].pdf_anchor_basis_revision &&
          parsed_anchor.excerpt_fingerprint == initial_records[0].excerpt_fingerprint,
      "parsed pdf anchors should preserve canonical rel_path, page, basis revision, and excerpt fingerprint");

  kernel::pdf::PdfAnchorValidationResult validation{};
  expect_ok(kernel::core::pdf_query::validate_live_pdf_anchor(
      handle,
      initial_records[0].anchor_serialized,
      validation));
  require_true(
      validation.state == kernel::pdf::PdfAnchorValidationState::Resolved,
      "freshly materialized pdf anchors should validate as resolved");

  const std::string metadata_only_bytes = make_pdf_bytes(
      "/Title (Renamed Anchor Doc) ",
      {"Page One Alpha", "Page Two Beta"},
      false);
  write_file_bytes(vault / "assets" / "anchor.pdf", metadata_only_bytes);

  require_eventually(
      [&]() {
        std::vector<kernel::storage::PdfAnchorRecord> updated_records;
        return try_query_pdf_anchors(handle, "assets/anchor.pdf", updated_records) &&
               updated_records.size() == 2 &&
               updated_records[0].pdf_anchor_basis_revision ==
                   initial_records[0].pdf_anchor_basis_revision &&
               updated_records[0].anchor_serialized ==
                   initial_records[0].anchor_serialized &&
               updated_records[1].pdf_anchor_basis_revision ==
                   initial_records[1].pdf_anchor_basis_revision &&
               updated_records[1].anchor_serialized ==
                   initial_records[1].anchor_serialized;
      },
      "pdf anchor basis revision should stay stable when only doc_title or outline metadata changes");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_pdf_anchor_validation_distinguishes_stale_unverifiable_unavailable_and_rebuild_reopen() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "assets");
  const std::string original_bytes =
      make_pdf_bytes("/Title (Anchor Validation) ", {"Original Page Text"}, false);
  write_file_bytes(vault / "assets" / "validation.pdf", original_bytes);

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string note =
      "# PDF Anchor Validation\n"
      "[Validation](assets/validation.pdf)\n";
  expect_ok(kernel_write_note(
      handle,
      "validation.md",
      note.data(),
      note.size(),
      nullptr,
      &metadata,
      &disposition));

  const auto initial_records = query_pdf_anchors(handle, "assets/validation.pdf");
  require_true(
      initial_records.size() == 1,
      "validation fixture should materialize exactly one page anchor");
  const std::string initial_serialized = initial_records[0].anchor_serialized;
  const std::string initial_basis_revision = initial_records[0].pdf_anchor_basis_revision;

  const std::string stale_bytes =
      make_pdf_bytes("/Title (Anchor Validation) ", {"Changed Page Text"}, false);
  write_file_bytes(vault / "assets" / "validation.pdf", stale_bytes);

  require_eventually(
      [&]() {
        std::vector<kernel::storage::PdfAnchorRecord> updated_records;
        if (!try_query_pdf_anchors(handle, "assets/validation.pdf", updated_records) ||
            updated_records.size() != 1 ||
            updated_records[0].pdf_anchor_basis_revision == initial_basis_revision) {
          return false;
        }

        kernel::pdf::PdfAnchorValidationResult validation{};
        if (kernel::core::pdf_query::validate_live_pdf_anchor(
                handle,
                initial_serialized,
                validation)
                .code != KERNEL_OK) {
          return false;
        }
        return validation.state == kernel::pdf::PdfAnchorValidationState::Stale;
      },
      "pdf anchor validation should report STALE when anchor-relevant page text changes");

  write_file_bytes(vault / "assets" / "validation.pdf", "not-a-pdf");
  require_eventually(
      [&]() {
        kernel::pdf::PdfAnchorValidationResult validation{};
        if (kernel::core::pdf_query::validate_live_pdf_anchor(
                handle,
                initial_serialized,
                validation)
                .code != KERNEL_OK) {
          return false;
        }
        return validation.state == kernel::pdf::PdfAnchorValidationState::Unverifiable;
      },
      "pdf anchor validation should report UNVERIFIABLE when the live PDF can no longer rebuild anchors");

  write_file_bytes(vault / "assets" / "validation.pdf", original_bytes);
  require_eventually(
      [&]() {
        std::vector<kernel::storage::PdfAnchorRecord> restored_records;
        if (!try_query_pdf_anchors(handle, "assets/validation.pdf", restored_records) ||
            restored_records.size() != 1 ||
            restored_records[0].anchor_serialized != initial_serialized) {
          return false;
        }

        kernel::pdf::PdfAnchorValidationResult validation{};
        if (kernel::core::pdf_query::validate_live_pdf_anchor(
                handle,
                initial_serialized,
                validation)
                .code != KERNEL_OK) {
          return false;
        }
        return validation.state == kernel::pdf::PdfAnchorValidationState::Resolved;
      },
      "pdf anchor substrate should rebuild the same canonical anchor for identical disk truth");

  expect_ok(kernel_rebuild_index(handle));
  {
    const auto rebuilt_records = query_pdf_anchors(handle, "assets/validation.pdf");
    require_true(
        rebuilt_records.size() == 1 &&
            rebuilt_records[0].anchor_serialized == initial_serialized,
        "rebuild should preserve canonical pdf anchor serialization for identical disk truth");
  }

  expect_ok(kernel_close(handle));
  handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  {
    const auto reopened_records = query_pdf_anchors(handle, "assets/validation.pdf");
    require_true(
        reopened_records.size() == 1 &&
            reopened_records[0].anchor_serialized == initial_serialized,
        "reopen should preserve canonical pdf anchor serialization for identical disk truth");
  }

  std::filesystem::remove(vault / "assets" / "validation.pdf");
  require_eventually(
      [&]() {
        kernel::pdf::PdfAnchorValidationResult validation{};
        if (kernel::core::pdf_query::validate_live_pdf_anchor(
                handle,
                initial_serialized,
                validation)
                .code != KERNEL_OK) {
          return false;
        }
        return validation.state == kernel::pdf::PdfAnchorValidationState::Unavailable;
      },
      "pdf anchor validation should report UNAVAILABLE when the live PDF attachment is missing");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_pdf_surface_anchor_tests() {
  test_pdf_anchor_serialization_roundtrips_and_ignores_unrelated_metadata_changes();
  test_pdf_anchor_validation_distinguishes_stale_unverifiable_unavailable_and_rebuild_reopen();
}
