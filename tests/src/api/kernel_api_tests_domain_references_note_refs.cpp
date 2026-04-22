// Reason: Keep Track 4 note->domain-reference coverage separate so the new
// generic substrate can evolve without bloating PDF-specific suites.

#include "api/kernel_api_domain_reference_suites.h"

#include "core/kernel_pdf_query_shared.h"
#include "kernel/c_api.h"
#include "support/test_support.h"

#include <filesystem>
#include <string>
#include <vector>

namespace {

struct DomainSourceRefSnapshot {
  std::string target_object_key;
  kernel_domain_selector_kind selector_kind = KERNEL_DOMAIN_SELECTOR_OPAQUE_DOMAIN_SELECTOR;
  std::string selector_serialized;
  std::string preview_text;
  std::string target_basis_revision;
  kernel_domain_ref_state state = KERNEL_DOMAIN_REF_UNRESOLVED;
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

bool try_snapshot_note_domain_source_refs(
    kernel_handle* handle,
    const char* note_rel_path,
    const size_t limit,
    std::vector<DomainSourceRefSnapshot>& out_refs) {
  kernel_domain_source_refs refs{};
  const kernel_status status =
      kernel_query_note_domain_source_refs(handle, note_rel_path, limit, &refs);
  if (status.code != KERNEL_OK) {
    kernel_free_domain_source_refs(&refs);
    return false;
  }

  out_refs.clear();
  out_refs.reserve(refs.count);
  for (size_t index = 0; index < refs.count; ++index) {
    out_refs.push_back(DomainSourceRefSnapshot{
        refs.refs[index].target_object_key == nullptr ? "" : refs.refs[index].target_object_key,
        refs.refs[index].selector_kind,
        refs.refs[index].selector_serialized == nullptr ? "" : refs.refs[index].selector_serialized,
        refs.refs[index].preview_text == nullptr ? "" : refs.refs[index].preview_text,
        refs.refs[index].target_basis_revision == nullptr ? "" : refs.refs[index].target_basis_revision,
        refs.refs[index].state});
  }
  kernel_free_domain_source_refs(&refs);
  return true;
}

std::vector<DomainSourceRefSnapshot> query_note_domain_source_refs(
    kernel_handle* handle,
    const char* note_rel_path,
    const size_t limit) {
  std::vector<DomainSourceRefSnapshot> refs;
  require_true(
      try_snapshot_note_domain_source_refs(handle, note_rel_path, limit, refs),
      "domain source ref query should succeed");
  return refs;
}

void test_note_domain_source_refs_surface_projects_pdf_source_refs_into_generic_shape() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "assets");
  write_file_bytes(vault / "assets" / "resolved.pdf", make_pdf_bytes("Resolved Page Text"));
  write_file_bytes(vault / "assets" / "stale.pdf", make_pdf_bytes("Original Stale Text"));

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string seed_note =
      "# Domain Ref Seed\n"
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
  const std::string resolved_basis = resolved_anchor_records[0].pdf_anchor_basis_revision;
  const std::string stale_anchor = stale_anchor_records[0].anchor_serialized;
  const std::string stale_excerpt = stale_anchor_records[0].excerpt_text;
  const std::string missing_anchor =
      "pdfa:v1|path=assets/missing.pdf|basis=missing-basis|page=1|xfp=missing-xfp";

  const std::string source_note =
      "# Domain Sources\n"
      "[Resolved](assets/resolved.pdf#anchor=" + resolved_anchor + ")\n"
      "[Missing](assets/missing.pdf#anchor=" + missing_anchor + ")\n"
      "[Stale](assets/stale.pdf#anchor=" + stale_anchor + ")\n"
      "[Broken](assets/resolved.pdf#anchor=not-a-canonical-anchor)\n";
  expect_ok(kernel_write_note(
      handle,
      "source.md",
      source_note.data(),
      source_note.size(),
      nullptr,
      &metadata,
      &disposition));

  auto refs = query_note_domain_source_refs(handle, "source.md", 8);
  require_true(refs.size() == 4, "note->domain refs should project every formal pdf source ref");
  require_true(
      refs[0].target_object_key == "dom:v1/pdf/assets%2Fresolved.pdf/generic/pdf_document" &&
          refs[0].selector_kind == KERNEL_DOMAIN_SELECTOR_OPAQUE_DOMAIN_SELECTOR &&
          refs[0].selector_serialized == resolved_anchor &&
          refs[0].preview_text == "Resolved Page Text" &&
          refs[0].target_basis_revision == resolved_basis &&
          refs[0].state == KERNEL_DOMAIN_REF_RESOLVED,
      "resolved domain refs should preserve target object key, selector, preview text, and basis revision");
  require_true(
      refs[1].target_object_key == "dom:v1/pdf/assets%2Fmissing.pdf/generic/pdf_document" &&
          refs[1].target_basis_revision == "missing-basis" &&
          refs[1].state == KERNEL_DOMAIN_REF_MISSING,
      "missing domain refs should preserve the parsed target basis revision and missing state");
  require_true(
      refs[2].target_object_key == "dom:v1/pdf/assets%2Fstale.pdf/generic/pdf_document" &&
          refs[2].preview_text == stale_excerpt &&
          refs[2].state == KERNEL_DOMAIN_REF_RESOLVED,
      "freshly stored canonical anchors should project as resolved domain refs before the PDF changes");
  require_true(
      refs[3].target_object_key == "dom:v1/pdf/assets%2Fresolved.pdf/generic/pdf_document" &&
          refs[3].target_basis_revision.empty() &&
          refs[3].state == KERNEL_DOMAIN_REF_UNRESOLVED,
      "malformed selectors should project as unresolved refs without synthetic basis data");

  write_file_bytes(vault / "assets" / "stale.pdf", make_pdf_bytes("Changed Stale Text"));
  require_eventually(
      [&]() {
        std::vector<DomainSourceRefSnapshot> updated_refs;
        if (!try_snapshot_note_domain_source_refs(handle, "source.md", 8, updated_refs) ||
            updated_refs.size() != 4) {
          return false;
        }

        return updated_refs[0].state == KERNEL_DOMAIN_REF_RESOLVED &&
               updated_refs[1].state == KERNEL_DOMAIN_REF_MISSING &&
               updated_refs[2].state == KERNEL_DOMAIN_REF_STALE &&
               updated_refs[2].preview_text == stale_excerpt &&
               updated_refs[3].state == KERNEL_DOMAIN_REF_UNRESOLVED;
      },
      "note->domain refs should degrade to STALE after anchor-relevant text changes while preserving stored preview text");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_note_domain_source_refs_surface_applies_limit_and_argument_contract() {
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

  const auto limited_refs = query_note_domain_source_refs(handle, "limit.md", 1);
  require_true(
      limited_refs.size() == 1 &&
          limited_refs[0].selector_serialized == limit_anchor_records[0].anchor_serialized,
      "note->domain refs should apply limit after stable source-order sorting");

  kernel_domain_source_refs refs{};
  kernel_status status = kernel_query_note_domain_source_refs(handle, "missing.md", 4, &refs);
  require_true(
      status.code == KERNEL_ERROR_NOT_FOUND,
      "note->domain refs should return NOT_FOUND for unknown notes");
  kernel_free_domain_source_refs(&refs);

  status = kernel_query_note_domain_source_refs(handle, "limit.md", 0, &refs);
  require_true(
      status.code == KERNEL_ERROR_INVALID_ARGUMENT,
      "note->domain refs should reject zero limits");

  status = kernel_query_note_domain_source_refs(handle, nullptr, 4, &refs);
  require_true(
      status.code == KERNEL_ERROR_INVALID_ARGUMENT,
      "note->domain refs should reject null note paths");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_domain_reference_note_ref_tests() {
  test_note_domain_source_refs_surface_projects_pdf_source_refs_into_generic_shape();
  test_note_domain_source_refs_surface_applies_limit_and_argument_contract();
}
