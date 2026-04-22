// Reason: Keep Track 3 Batch 3 PDF->note referrer coverage separate so
// referrer sorting and state semantics stay isolated from note-side suites.

#include "api/kernel_api_pdf_surface_suites.h"

#include "core/kernel_pdf_query_shared.h"
#include "kernel/c_api.h"
#include "support/test_support.h"

#include <filesystem>
#include <string>
#include <vector>

namespace {

struct PdfReferrerSnapshot {
  std::string note_rel_path;
  std::string note_title;
  std::string anchor_serialized;
  std::string excerpt_text;
  std::uint64_t page = 0;
  kernel_pdf_ref_state state = KERNEL_PDF_REF_UNRESOLVED;
};

std::string make_pdf_bytes(std::string_view page_text) {
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

bool try_snapshot_pdf_referrers(
    kernel_handle* handle,
    const char* attachment_rel_path,
    const size_t limit,
    std::vector<PdfReferrerSnapshot>& out_referrers) {
  kernel_pdf_referrers referrers{};
  const kernel_status status =
      kernel_query_pdf_referrers(handle, attachment_rel_path, limit, &referrers);
  if (status.code != KERNEL_OK) {
    kernel_free_pdf_referrers(&referrers);
    return false;
  }

  out_referrers.clear();
  out_referrers.reserve(referrers.count);
  for (size_t index = 0; index < referrers.count; ++index) {
    out_referrers.push_back(PdfReferrerSnapshot{
        referrers.referrers[index].note_rel_path == nullptr
            ? ""
            : referrers.referrers[index].note_rel_path,
        referrers.referrers[index].note_title == nullptr
            ? ""
            : referrers.referrers[index].note_title,
        referrers.referrers[index].anchor_serialized == nullptr
            ? ""
            : referrers.referrers[index].anchor_serialized,
        referrers.referrers[index].excerpt_text == nullptr
            ? ""
            : referrers.referrers[index].excerpt_text,
        referrers.referrers[index].page,
        referrers.referrers[index].state});
  }
  kernel_free_pdf_referrers(&referrers);
  return true;
}

std::vector<PdfReferrerSnapshot> query_pdf_referrers(
    kernel_handle* handle,
    const char* attachment_rel_path,
    const size_t limit) {
  std::vector<PdfReferrerSnapshot> referrers;
  require_true(
      try_snapshot_pdf_referrers(handle, attachment_rel_path, limit, referrers),
      "pdf referrer query should succeed");
  return referrers;
}

void test_pdf_referrers_surface_reports_sorted_note_refs_and_excludes_plain_attachment_links() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "assets");
  write_file_bytes(vault / "assets" / "shared.pdf", make_pdf_bytes("Shared Anchor Text"));
  write_file_bytes(vault / "assets" / "attachment-only.pdf", make_pdf_bytes("Attachment Only Text"));
  write_file_bytes(vault / "assets" / "unreferenced.pdf", make_pdf_bytes("Unreferenced Text"));

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string seed_note =
      "# Referrer Seed\n"
      "[Shared](assets/shared.pdf)\n";
  expect_ok(kernel_write_note(
      handle,
      "seed.md",
      seed_note.data(),
      seed_note.size(),
      nullptr,
      &metadata,
      &disposition));
  const auto anchor_records = query_pdf_anchors(handle, "assets/shared.pdf");
  require_true(anchor_records.size() == 1, "shared pdf fixture should materialize one anchor");

  const std::string shared_anchor = anchor_records[0].anchor_serialized;
  const std::string shared_excerpt = anchor_records[0].excerpt_text;

  const std::string alpha_note =
      "# Alpha Referrer\n"
      "[Resolved](assets/shared.pdf#anchor=" + shared_anchor + ")\n"
      "[Broken](assets/shared.pdf#anchor=broken-anchor)\n"
      "[Plain](assets/shared.pdf)\n";
  expect_ok(kernel_write_note(
      handle,
      "alpha.md",
      alpha_note.data(),
      alpha_note.size(),
      nullptr,
      &metadata,
      &disposition));

  const std::string beta_note =
      "# Beta Referrer\n"
      "[Resolved](assets/shared.pdf#anchor=" + shared_anchor + ")\n"
      "[AttachmentOnly](assets/attachment-only.pdf)\n";
  expect_ok(kernel_write_note(
      handle,
      "beta.md",
      beta_note.data(),
      beta_note.size(),
      nullptr,
      &metadata,
      &disposition));

  auto referrers = query_pdf_referrers(handle, "assets/shared.pdf", 8);
  require_true(referrers.size() == 3, "pdf referrers should include only formal source refs");
  require_true(
      referrers[0].note_rel_path == "alpha.md" &&
          referrers[0].state == KERNEL_PDF_REF_RESOLVED &&
          referrers[0].page == 1 &&
          referrers[0].excerpt_text == shared_excerpt,
      "pdf referrers should sort by note path then ordinal and preserve resolved excerpt text");
  require_true(
      referrers[1].note_rel_path == "alpha.md" &&
          referrers[1].state == KERNEL_PDF_REF_UNRESOLVED &&
          referrers[1].page == 0,
      "pdf referrers should preserve per-note ordinal order for unresolved refs");
  require_true(
      referrers[2].note_rel_path == "beta.md" &&
          referrers[2].state == KERNEL_PDF_REF_RESOLVED,
      "pdf referrers should include every note-side source ref for the live PDF path");

  const auto attachment_only_referrers =
      query_pdf_referrers(handle, "assets/attachment-only.pdf", 8);
  require_true(
      attachment_only_referrers.empty(),
      "pdf referrers should return an empty list for live PDFs with no formal source refs");

  write_file_bytes(vault / "assets" / "shared.pdf", make_pdf_bytes("Changed Shared Anchor Text"));
  require_eventually(
      [&]() {
        std::vector<PdfReferrerSnapshot> updated_referrers;
        if (!try_snapshot_pdf_referrers(handle, "assets/shared.pdf", 8, updated_referrers) ||
            updated_referrers.size() != 3) {
          return false;
        }

        return updated_referrers[0].state == KERNEL_PDF_REF_STALE &&
               updated_referrers[0].excerpt_text == shared_excerpt &&
               updated_referrers[1].state == KERNEL_PDF_REF_UNRESOLVED &&
               updated_referrers[2].state == KERNEL_PDF_REF_STALE;
      },
      "pdf referrers should degrade resolved anchors to STALE after anchor-relevant text changes while preserving stored excerpt snapshots");

  kernel_pdf_referrers raw_referrers{};
  kernel_status status = kernel_query_pdf_referrers(handle, "assets/unreferenced.pdf", 4, &raw_referrers);
  require_true(
      status.code == KERNEL_ERROR_NOT_FOUND,
      "pdf referrers should return NOT_FOUND for unreferenced disk PDFs");
  kernel_free_pdf_referrers(&raw_referrers);

  status = kernel_query_pdf_referrers(handle, "assets/shared.pdf", 1, &raw_referrers);
  require_true(
      status.code == KERNEL_OK && raw_referrers.count == 1,
      "pdf referrers should apply limit after stable ordering");
  kernel_free_pdf_referrers(&raw_referrers);

  status = kernel_query_pdf_referrers(handle, "assets/shared.pdf", 0, &raw_referrers);
  require_true(
      status.code == KERNEL_ERROR_INVALID_ARGUMENT,
      "pdf referrers should reject zero limits");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_pdf_surface_reference_referrer_tests() {
  test_pdf_referrers_surface_reports_sorted_note_refs_and_excludes_plain_attachment_links();
}
