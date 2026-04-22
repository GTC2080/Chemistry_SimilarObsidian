// Reason: Keep Track 4 domain-object->note referrer coverage separate so the
// new generic substrate can evolve without bloating PDF-specific suites.

#include "api/kernel_api_domain_reference_suites.h"

#include "core/kernel_pdf_query_shared.h"
#include "kernel/c_api.h"
#include "support/test_support.h"

#include <filesystem>
#include <string>
#include <vector>

namespace {

struct DomainReferrerSnapshot {
  std::string note_rel_path;
  std::string note_title;
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

bool try_snapshot_domain_referrers(
    kernel_handle* handle,
    const char* domain_object_key,
    const size_t limit,
    std::vector<DomainReferrerSnapshot>& out_referrers) {
  kernel_domain_referrers referrers{};
  const kernel_status status =
      kernel_query_domain_object_referrers(handle, domain_object_key, limit, &referrers);
  if (status.code != KERNEL_OK) {
    kernel_free_domain_referrers(&referrers);
    return false;
  }

  out_referrers.clear();
  out_referrers.reserve(referrers.count);
  for (size_t index = 0; index < referrers.count; ++index) {
    out_referrers.push_back(DomainReferrerSnapshot{
        referrers.referrers[index].note_rel_path == nullptr ? "" : referrers.referrers[index].note_rel_path,
        referrers.referrers[index].note_title == nullptr ? "" : referrers.referrers[index].note_title,
        referrers.referrers[index].target_object_key == nullptr ? "" : referrers.referrers[index].target_object_key,
        referrers.referrers[index].selector_kind,
        referrers.referrers[index].selector_serialized == nullptr ? "" : referrers.referrers[index].selector_serialized,
        referrers.referrers[index].preview_text == nullptr ? "" : referrers.referrers[index].preview_text,
        referrers.referrers[index].target_basis_revision == nullptr ? "" : referrers.referrers[index].target_basis_revision,
        referrers.referrers[index].state});
  }
  kernel_free_domain_referrers(&referrers);
  return true;
}

std::vector<DomainReferrerSnapshot> query_domain_referrers(
    kernel_handle* handle,
    const char* domain_object_key,
    const size_t limit) {
  std::vector<DomainReferrerSnapshot> referrers;
  require_true(
      try_snapshot_domain_referrers(handle, domain_object_key, limit, referrers),
      "domain referrer query should succeed");
  return referrers;
}

void test_domain_object_referrers_surface_projects_pdf_referrers_into_generic_shape() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "assets");
  write_file_bytes(vault / "assets" / "shared.pdf", make_pdf_bytes("Shared Anchor Text"));
  write_file_bytes(vault / "assets" / "attachment-only.pdf", make_pdf_bytes("Attachment Only Text"));

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
  const std::string shared_basis = anchor_records[0].pdf_anchor_basis_revision;
  const std::string shared_excerpt = anchor_records[0].excerpt_text;
  const std::string pdf_domain_object_key =
      "dom:v1/pdf/assets%2Fshared.pdf/generic/pdf_document";
  const std::string attachment_domain_object_key =
      "dom:v1/attachment/assets%2Fshared.pdf/generic/attachment_resource";

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

  auto referrers = query_domain_referrers(handle, pdf_domain_object_key.c_str(), 8);
  require_true(referrers.size() == 3, "domain referrers should include only formal projected source refs");
  require_true(
      referrers[0].note_rel_path == "alpha.md" &&
          referrers[0].target_object_key == pdf_domain_object_key &&
          referrers[0].selector_kind == KERNEL_DOMAIN_SELECTOR_OPAQUE_DOMAIN_SELECTOR &&
          referrers[0].preview_text == shared_excerpt &&
          referrers[0].target_basis_revision == shared_basis &&
          referrers[0].state == KERNEL_DOMAIN_REF_RESOLVED,
      "domain referrers should preserve note ordering, target object key, preview text, and basis revision");
  require_true(
      referrers[1].note_rel_path == "alpha.md" &&
          referrers[1].state == KERNEL_DOMAIN_REF_UNRESOLVED,
      "domain referrers should preserve unresolved selectors");
  require_true(
      referrers[2].note_rel_path == "beta.md" &&
          referrers[2].state == KERNEL_DOMAIN_REF_RESOLVED,
      "domain referrers should include every note-side projected ref");

  const auto attachment_referrers = query_domain_referrers(
      handle,
      attachment_domain_object_key.c_str(),
      8);
  require_true(
      attachment_referrers.empty(),
      "attachment_resource objects should currently expose an empty generic referrer list");

  write_file_bytes(vault / "assets" / "shared.pdf", make_pdf_bytes("Changed Shared Anchor Text"));
  require_eventually(
      [&]() {
        std::vector<DomainReferrerSnapshot> updated_referrers;
        if (!try_snapshot_domain_referrers(handle, pdf_domain_object_key.c_str(), 8, updated_referrers) ||
            updated_referrers.size() != 3) {
          return false;
        }

        return updated_referrers[0].state == KERNEL_DOMAIN_REF_STALE &&
               updated_referrers[0].preview_text == shared_excerpt &&
               updated_referrers[1].state == KERNEL_DOMAIN_REF_UNRESOLVED &&
               updated_referrers[2].state == KERNEL_DOMAIN_REF_STALE;
      },
      "domain referrers should degrade resolved selectors to STALE after anchor-relevant text changes while preserving stored preview text");

  kernel_domain_referrers raw_referrers{};
  kernel_status status =
      kernel_query_domain_object_referrers(
          handle,
          "dom:v1/pdf/assets%2Funreferenced.pdf/generic/pdf_document",
          4,
          &raw_referrers);
  require_true(
      status.code == KERNEL_ERROR_NOT_FOUND,
      "domain referrers should return NOT_FOUND for non-live domain objects");
  kernel_free_domain_referrers(&raw_referrers);

  status = kernel_query_domain_object_referrers(handle, pdf_domain_object_key.c_str(), 1, &raw_referrers);
  require_true(
      status.code == KERNEL_OK && raw_referrers.count == 1,
      "domain referrers should apply limit after stable ordering");
  kernel_free_domain_referrers(&raw_referrers);

  status = kernel_query_domain_object_referrers(handle, pdf_domain_object_key.c_str(), 0, &raw_referrers);
  require_true(
      status.code == KERNEL_ERROR_INVALID_ARGUMENT,
      "domain referrers should reject zero limits");

  status = kernel_query_domain_object_referrers(
      handle,
      "dom:v1/pdf/assets/shared.pdf/generic/pdf_document",
      4,
      &raw_referrers);
  require_true(
      status.code == KERNEL_ERROR_INVALID_ARGUMENT,
      "domain referrers should reject noncanonical domain object keys");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_domain_reference_referrer_tests() {
  test_domain_object_referrers_surface_projects_pdf_referrers_into_generic_shape();
}
