// Reason: Keep Track 4 domain diagnostics snapshot coverage separate so
// support-bundle contract checks stay focused and compact.

#include "kernel/c_api.h"

#include "api/kernel_api_domain_diagnostics_suites.h"
#include "api/kernel_api_pdf_test_helpers.h"
#include "api/kernel_api_test_support.h"
#include "support/test_support.h"

#include <filesystem>
#include <string>
#include <vector>

namespace {

void test_export_diagnostics_reports_domain_contract_snapshot() {
  const auto vault = make_temp_vault();
  const auto export_path = vault / "domain-diagnostics-snapshot.json";
  std::filesystem::create_directories(vault / "assets");
  write_file_bytes(vault / "assets" / "diagram.png", "png-bytes");
  write_file_bytes(vault / "assets" / "ready.pdf", make_text_pdf_bytes("Ready PDF Text"));
  write_file_bytes(vault / "assets" / "invalid.pdf", "not-a-pdf");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "domain diagnostics snapshot test should start from READY");

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string seed_note =
      "# Domain Diagnostics Seed\n"
      "![Diagram](assets/diagram.png)\n"
      "[Ready](assets/ready.pdf)\n"
      "[Invalid](assets/invalid.pdf)\n"
      "[Missing](assets/missing.pdf)\n";
  expect_ok(kernel_write_note(
      handle,
      "domain-diagnostics-seed.md",
      seed_note.data(),
      seed_note.size(),
      nullptr,
      &metadata,
      &disposition));

  const auto anchor_records = query_pdf_anchor_records(handle, "assets/ready.pdf");
  require_true(anchor_records.size() == 1, "domain diagnostics snapshot should materialize one pdf anchor");

  const std::string missing_anchor =
      "pdfa:v1|path=assets/missing.pdf|basis=missing-basis|page=1|xfp=missing-xfp";
  const std::string source_note =
      "# Domain Diagnostics Source\n"
      "[Resolved](assets/ready.pdf#anchor=" + anchor_records[0].anchor_serialized + ")\n"
      "[Missing](assets/missing.pdf#anchor=" + missing_anchor + ")\n"
      "[Broken](assets/ready.pdf#anchor=broken-anchor)\n";
  expect_ok(kernel_write_note(
      handle,
      "domain-diagnostics-source.md",
      source_note.data(),
      source_note.size(),
      nullptr,
      &metadata,
      &disposition));

  expect_ok(kernel_export_diagnostics(handle, export_path.string().c_str()));
  const std::string exported = read_file_text(export_path);
  require_true(
      exported.find("\"domain_contract_revision\":\"track4_batch3_domain_extension_contract_v1\"") !=
          std::string::npos,
      "domain diagnostics should export the current domain contract revision");
  require_true(
      exported.find("\"domain_diagnostics_revision\":\"track4_batch4_domain_diagnostics_v1\"") !=
          std::string::npos,
      "domain diagnostics should export the current diagnostics revision");
  require_true(
      exported.find("\"domain_benchmark_gate_revision\":\"track4_batch4_domain_query_gates_v1\"") !=
          std::string::npos,
      "domain diagnostics should export the current benchmark-gate revision");
  require_true(
      exported.find("\"domain_namespace_summary\":\"generic\"") != std::string::npos,
      "domain diagnostics should summarize the current public namespace set");
  require_true(
      exported.find("\"domain_subtype_summary\":\"generic.attachment_resource,generic.pdf_document\"") !=
          std::string::npos,
      "domain diagnostics should summarize the current public subtype set");
  require_true(
      exported.find("\"domain_source_reference_summary\":\"generic.pdf_document_projection\"") !=
          std::string::npos,
      "domain diagnostics should summarize the current generic source-reference substrate");
  require_true(
      exported.find("\"domain_attachment_metadata_entry_count\":12") != std::string::npos,
      "domain diagnostics should export attachment-carrier domain metadata entry counts");
  require_true(
      exported.find("\"domain_pdf_metadata_entry_count\":21") != std::string::npos,
      "domain diagnostics should export pdf-carrier domain metadata entry counts");
  require_true(
      exported.find("\"domain_object_count\":7") != std::string::npos,
      "domain diagnostics should export total public domain object count");
  require_true(
      exported.find("\"domain_source_ref_count\":3") != std::string::npos,
      "domain diagnostics should export total generic domain source-ref count");
  require_true(
      exported.find("\"domain_source_ref_resolved_count\":1") != std::string::npos,
      "domain diagnostics should export resolved domain source-ref count");
  require_true(
      exported.find("\"domain_source_ref_missing_count\":1") != std::string::npos,
      "domain diagnostics should export missing domain source-ref count");
  require_true(
      exported.find("\"domain_source_ref_stale_count\":0") != std::string::npos,
      "domain diagnostics should export zero stale domain source refs in the clean seeded case");
  require_true(
      exported.find("\"domain_source_ref_unresolved_count\":1") != std::string::npos,
      "domain diagnostics should export unresolved domain source-ref count");
  require_true(
      exported.find("\"domain_source_ref_unsupported_count\":0") != std::string::npos,
      "domain diagnostics should export zero unsupported domain source refs in Batch 4");
  require_true(
      exported.find("\"domain_unresolved_summary\":\"domain_source_refs\"") != std::string::npos,
      "domain diagnostics should summarize unresolved domain refs when broken selectors are present");
  require_true(
      exported.find("\"domain_stale_summary\":\"clean\"") != std::string::npos,
      "domain diagnostics should keep stale summary clean before anchor drift");
  require_true(
      exported.find("\"domain_unsupported_summary\":\"clean\"") != std::string::npos,
      "domain diagnostics should keep unsupported summary clean when no unsupported refs are projected");
  require_true(
      exported.find("\"capability_track_status_summary\":\"domain_metadata=gated;domain_objects=gated;domain_refs=gated\"") !=
          std::string::npos,
      "domain diagnostics should export the current capability-track status summary");
  require_true(
      exported.find("\"last_domain_recount_reason\":\"\"") != std::string::npos,
      "domain diagnostics should leave last_domain_recount_reason empty before rebuild or full rescan");
  require_true(
      exported.find("\"last_domain_recount_at_ns\":0") != std::string::npos,
      "domain diagnostics should leave last_domain_recount_at_ns zero before rebuild or full rescan");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_domain_diagnostics_snapshot_tests() {
  test_export_diagnostics_reports_domain_contract_snapshot();
}
