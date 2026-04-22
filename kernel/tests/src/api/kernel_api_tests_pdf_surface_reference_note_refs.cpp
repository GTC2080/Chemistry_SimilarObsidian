// Reason: Keep Track 3 Batch 3 note->PDF source-reference coverage separate so
// source-ref contract changes do not bloat metadata or referrer suites.

#include "api/kernel_api_pdf_surface_suites.h"

#include "core/kernel_pdf_query_shared.h"
#include "kernel/c_api.h"
#include "support/test_support.h"

#include <filesystem>
#include <string>
#include <vector>

namespace {

struct PdfSourceRefSnapshot {
  std::string pdf_rel_path;
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

bool try_snapshot_note_pdf_source_refs(
    kernel_handle* handle,
    const char* note_rel_path,
    const size_t limit,
    std::vector<PdfSourceRefSnapshot>& out_refs) {
  kernel_pdf_source_refs refs{};
  const kernel_status status =
      kernel_query_note_pdf_source_refs(handle, note_rel_path, limit, &refs);
  if (status.code != KERNEL_OK) {
    kernel_free_pdf_source_refs(&refs);
    return false;
  }

  out_refs.clear();
  out_refs.reserve(refs.count);
  for (size_t index = 0; index < refs.count; ++index) {
    out_refs.push_back(PdfSourceRefSnapshot{
        refs.refs[index].pdf_rel_path == nullptr ? "" : refs.refs[index].pdf_rel_path,
        refs.refs[index].anchor_serialized == nullptr ? "" : refs.refs[index].anchor_serialized,
        refs.refs[index].excerpt_text == nullptr ? "" : refs.refs[index].excerpt_text,
        refs.refs[index].page,
        refs.refs[index].state});
  }
  kernel_free_pdf_source_refs(&refs);
  return true;
}

std::vector<PdfSourceRefSnapshot> query_note_pdf_source_refs(
    kernel_handle* handle,
    const char* note_rel_path,
    const size_t limit) {
  std::vector<PdfSourceRefSnapshot> refs;
  require_true(
      try_snapshot_note_pdf_source_refs(handle, note_rel_path, limit, refs),
      "pdf source ref query should succeed");
  return refs;
}

void test_note_pdf_source_refs_surface_reports_resolved_missing_stale_and_unresolved_states() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "assets");
  write_file_bytes(vault / "assets" / "resolved.pdf", make_pdf_bytes("Resolved Page Text"));
  write_file_bytes(vault / "assets" / "stale.pdf", make_pdf_bytes("Original Stale Text"));

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string seed_note =
      "# PDF Anchor Seed\n"
      "[Resolved](assets/resolved.pdf)\n"
      "[Stale](assets/stale.pdf)\n";
  expect_ok(kernel_write_note(
      handle,
      "seed.md",
      seed_note.data(),
      seed_note.size(),
      nullptr,
      &metadata,
      &disposition));

  const auto resolved_anchor_records = query_pdf_anchors(handle, "assets/resolved.pdf");
  const auto stale_anchor_records = query_pdf_anchors(handle, "assets/stale.pdf");
  require_true(
      resolved_anchor_records.size() == 1 && stale_anchor_records.size() == 1,
      "seed note should materialize one anchor for each single-page PDF");

  const std::string resolved_anchor = resolved_anchor_records[0].anchor_serialized;
  const std::string stale_anchor = stale_anchor_records[0].anchor_serialized;
  const std::string stale_excerpt = stale_anchor_records[0].excerpt_text;
  const std::string missing_anchor =
      "pdfa:v1|path=assets/missing.pdf|basis=missing-basis|page=1|xfp=missing-xfp";

  const std::string source_note =
      "# PDF Sources\n"
      "[Resolved](assets/resolved.pdf#anchor=" + resolved_anchor + ")\n"
      "[Missing](assets/missing.pdf#anchor=" + missing_anchor + ")\n"
      "[Stale](assets/stale.pdf#anchor=" + stale_anchor + ")\n"
      "[Broken](assets/resolved.pdf#anchor=not-a-canonical-anchor)\n"
      "[Plain](assets/resolved.pdf)\n";
  expect_ok(kernel_write_note(
      handle,
      "source.md",
      source_note.data(),
      source_note.size(),
      nullptr,
      &metadata,
      &disposition));

  auto refs = query_note_pdf_source_refs(handle, "source.md", 8);
  require_true(refs.size() == 4, "note->pdf refs should include only formal #anchor references");
  require_true(
      refs[0].pdf_rel_path == "assets/resolved.pdf" &&
          refs[0].state == KERNEL_PDF_REF_RESOLVED &&
          refs[0].page == 1 &&
          refs[0].excerpt_text == "Resolved Page Text",
      "resolved pdf source refs should expose normalized rel_path, page, and excerpt");
  require_true(
      refs[1].pdf_rel_path == "assets/missing.pdf" &&
          refs[1].state == KERNEL_PDF_REF_MISSING &&
          refs[1].page == 1 &&
          refs[1].excerpt_text.empty(),
      "missing pdf source refs should surface the frozen missing state without excerpt text");
  require_true(
      refs[2].pdf_rel_path == "assets/stale.pdf" &&
          refs[2].state == KERNEL_PDF_REF_RESOLVED &&
          refs[2].page == 1 &&
          refs[2].excerpt_text == stale_excerpt,
      "freshly stored canonical anchors should resolve before the PDF changes");
  require_true(
      refs[3].pdf_rel_path == "assets/resolved.pdf" &&
          refs[3].state == KERNEL_PDF_REF_UNRESOLVED &&
          refs[3].page == 0 &&
          refs[3].excerpt_text.empty(),
      "malformed anchors should surface as unresolved without synthetic page or excerpt data");

  write_file_bytes(vault / "assets" / "stale.pdf", make_pdf_bytes("Changed Stale Text"));
  require_eventually(
      [&]() {
        std::vector<PdfSourceRefSnapshot> updated_refs;
        if (!try_snapshot_note_pdf_source_refs(handle, "source.md", 8, updated_refs) ||
            updated_refs.size() != 4) {
          return false;
        }

        return updated_refs[0].state == KERNEL_PDF_REF_RESOLVED &&
               updated_refs[1].state == KERNEL_PDF_REF_MISSING &&
               updated_refs[2].state == KERNEL_PDF_REF_STALE &&
               updated_refs[2].excerpt_text == stale_excerpt &&
               updated_refs[3].state == KERNEL_PDF_REF_UNRESOLVED;
      },
      "note->pdf refs should degrade to STALE after anchor-relevant page text changes while keeping the stored excerpt snapshot");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_note_pdf_source_refs_surface_applies_limit_and_argument_contract() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "assets");
  write_file_bytes(vault / "assets" / "limit.pdf", make_pdf_bytes("Limit Text"));

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string seed_note =
      "# Limit Seed\n"
      "[Limit](assets/limit.pdf)\n";
  expect_ok(kernel_write_note(
      handle,
      "limit-seed.md",
      seed_note.data(),
      seed_note.size(),
      nullptr,
      &metadata,
      &disposition));
  const auto limit_anchor_records = query_pdf_anchors(handle, "assets/limit.pdf");
  require_true(limit_anchor_records.size() == 1, "limit fixture should materialize one anchor");

  const std::string anchored_note =
      "# Limit Note\n"
      "[First](assets/limit.pdf#anchor=" + limit_anchor_records[0].anchor_serialized + ")\n"
      "[Second](assets/limit.pdf#anchor=broken-anchor)\n";
  expect_ok(kernel_write_note(
      handle,
      "limit.md",
      anchored_note.data(),
      anchored_note.size(),
      nullptr,
      &metadata,
      &disposition));

  const auto limited_refs = query_note_pdf_source_refs(handle, "limit.md", 1);
  require_true(
      limited_refs.size() == 1 &&
          limited_refs[0].anchor_serialized == limit_anchor_records[0].anchor_serialized,
      "note->pdf refs should apply limit after stable source-order sorting");

  kernel_pdf_source_refs refs{};
  kernel_status status = kernel_query_note_pdf_source_refs(handle, "missing.md", 4, &refs);
  require_true(
      status.code == KERNEL_ERROR_NOT_FOUND,
      "note->pdf refs should return NOT_FOUND for unknown notes");
  kernel_free_pdf_source_refs(&refs);

  status = kernel_query_note_pdf_source_refs(handle, "limit.md", 0, &refs);
  require_true(
      status.code == KERNEL_ERROR_INVALID_ARGUMENT,
      "note->pdf refs should reject zero limits");

  status = kernel_query_note_pdf_source_refs(handle, nullptr, 4, &refs);
  require_true(
      status.code == KERNEL_ERROR_INVALID_ARGUMENT,
      "note->pdf refs should reject null note paths");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_pdf_surface_reference_note_ref_tests() {
  test_note_pdf_source_refs_surface_reports_resolved_missing_stale_and_unresolved_states();
  test_note_pdf_source_refs_surface_applies_limit_and_argument_contract();
}
