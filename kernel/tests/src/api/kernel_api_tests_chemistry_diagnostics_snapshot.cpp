// Reason: Keep Track 5 chemistry diagnostics snapshot coverage separate so
// support-bundle contract checks stay focused and compact.

#include "kernel/c_api.h"

#include "api/kernel_api_chemistry_diagnostics_suites.h"
#include "api/kernel_api_test_support.h"
#include "chemistry/chemistry_spectrum_metadata.h"
#include "chemistry/chemistry_spectrum_selector.h"
#include "support/test_support.h"
#include "vault/revision.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace {

std::string make_jcamp_bytes(
    std::string_view title,
    std::string_view datatype,
    std::string_view xunits,
    std::string_view yunits,
    const std::uint64_t point_count) {
  return "##JCAMP-DX=5.01\n"
         "##TITLE=" + std::string(title) + "\n"
         "##DATA TYPE=" + std::string(datatype) + "\n"
         "##XUNITS=" + std::string(xunits) + "\n"
         "##YUNITS=" + std::string(yunits) + "\n"
         "##NPOINTS=" + std::to_string(point_count) + "\n";
}

std::string build_whole_selector_for_file(
    const std::filesystem::path& vault,
    const char* rel_path) {
  const std::string bytes = read_file_text(vault / std::filesystem::path(rel_path));
  const std::string attachment_content_revision =
      kernel::vault::compute_content_revision(bytes);
  const auto parsed =
      kernel::chemistry::extract_spectrum_metadata(rel_path, bytes, attachment_content_revision);
  require_true(
      parsed.status == kernel::chemistry::SpectrumParseStatus::Ready,
      "chemistry diagnostics whole selector fixture should parse");

  kernel::chemistry::ParsedChemSpectrumSelector selector;
  selector.kind = kernel::chemistry::ChemSpectrumSelectorKind::WholeSpectrum;
  selector.chemistry_selector_basis_revision =
      kernel::chemistry::build_chemistry_selector_basis_revision(
          parsed.metadata.attachment_content_revision,
          kernel::chemistry::build_normalized_spectrum_basis(parsed.metadata));
  return kernel::chemistry::serialize_chem_spectrum_selector(selector);
}

std::string build_x_range_selector_for_file(
    const std::filesystem::path& vault,
    const char* rel_path,
    std::string_view start,
    std::string_view end) {
  const std::string bytes = read_file_text(vault / std::filesystem::path(rel_path));
  const std::string attachment_content_revision =
      kernel::vault::compute_content_revision(bytes);
  const auto parsed =
      kernel::chemistry::extract_spectrum_metadata(rel_path, bytes, attachment_content_revision);
  require_true(
      parsed.status == kernel::chemistry::SpectrumParseStatus::Ready,
      "chemistry diagnostics x-range selector fixture should parse");

  std::string normalized_start;
  std::string normalized_end;
  require_true(
      kernel::chemistry::normalize_selector_decimal(start, normalized_start) &&
          kernel::chemistry::normalize_selector_decimal(end, normalized_end),
      "chemistry diagnostics x-range selector fixture should normalize decimals");

  kernel::chemistry::ParsedChemSpectrumSelector selector;
  selector.kind = kernel::chemistry::ChemSpectrumSelectorKind::XRange;
  selector.chemistry_selector_basis_revision =
      kernel::chemistry::build_chemistry_selector_basis_revision(
          parsed.metadata.attachment_content_revision,
          kernel::chemistry::build_normalized_spectrum_basis(parsed.metadata));
  selector.start = normalized_start;
  selector.end = normalized_end;
  selector.unit = parsed.metadata.x_axis_unit;
  return kernel::chemistry::serialize_chem_spectrum_selector(selector);
}

bool chemistry_ref_states_match(
    kernel_handle* handle,
    const char* note_rel_path) {
  kernel_chem_spectrum_source_refs refs{};
  const kernel_status status =
      kernel_query_note_chem_spectrum_refs(handle, note_rel_path, 8, &refs);
  if (status.code != KERNEL_OK) {
    kernel_free_chem_spectrum_source_refs(&refs);
    return false;
  }

  const bool matches =
      refs.count == 6 &&
      refs.refs[0].state == KERNEL_DOMAIN_REF_RESOLVED &&
      refs.refs[1].state == KERNEL_DOMAIN_REF_MISSING &&
      refs.refs[2].state == KERNEL_DOMAIN_REF_STALE &&
      refs.refs[3].state == KERNEL_DOMAIN_REF_UNRESOLVED &&
      refs.refs[4].state == KERNEL_DOMAIN_REF_UNSUPPORTED &&
      refs.refs[5].state == KERNEL_DOMAIN_REF_UNRESOLVED;
  kernel_free_chem_spectrum_source_refs(&refs);
  return matches;
}

void test_export_diagnostics_reports_chemistry_contract_snapshot() {
  const auto vault = make_temp_vault();
  const auto export_path = vault / "chemistry-diagnostics-snapshot.json";
  std::filesystem::create_directories(vault / "spectra");
  write_file_bytes(
      vault / "spectra" / "ready.jdx",
      make_jcamp_bytes("Ready", "NMR SPECTRUM", "PPM", "INTENSITY", 4));
  write_file_bytes(
      vault / "spectra" / "stale.jdx",
      make_jcamp_bytes("Stale", "NMR SPECTRUM", "PPM", "INTENSITY", 4));
  write_file_bytes(vault / "spectra" / "broken.csv", "x,y\n1,2\n");
  write_file_bytes(vault / "spectra" / "unsupported.sdf", "sdf-bytes");

  const std::string ready_selector =
      build_whole_selector_for_file(vault, "spectra/ready.jdx");
  const std::string stale_selector =
      build_x_range_selector_for_file(vault, "spectra/stale.jdx", "1", "2");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "chemistry diagnostics snapshot test should start from READY");

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string note =
      "# Chemistry Diagnostics\n"
      "[Resolved](spectra/ready.jdx#chemsel=" + ready_selector + ")\n"
      "[Missing](spectra/missing.jdx#chemsel=chemsel:v1|kind=whole|basis=missing-basis)\n"
      "[Stale](spectra/stale.jdx#chemsel=" + stale_selector + ")\n"
      "[BrokenCarrier](spectra/broken.csv#chemsel=chemsel:v1|kind=whole|basis=broken-basis)\n"
      "[Unsupported](spectra/unsupported.sdf#chemsel=chemsel:v1|kind=whole|basis=unsupported-basis)\n"
      "[BrokenSelector](spectra/ready.jdx#chemsel=not-a-selector)\n";
  expect_ok(kernel_write_note(
      handle,
      "chemistry-diagnostics.md",
      note.data(),
      note.size(),
      nullptr,
      &metadata,
      &disposition));

  write_file_bytes(
      vault / "spectra" / "stale.jdx",
      make_jcamp_bytes("Stale Changed", "NMR SPECTRUM", "PPM", "INTENSITY", 6));
  require_eventually(
      [&]() { return chemistry_ref_states_match(handle, "chemistry-diagnostics.md"); },
      "chemistry diagnostics snapshot test should settle chemistry ref states before export");

  expect_ok(kernel_export_diagnostics(handle, export_path.string().c_str()));
  const std::string exported = read_file_text(export_path);
  require_true(
      exported.find("\"chemistry_contract_revision\":\"track5_batch3_chemistry_source_reference_v1\"") !=
          std::string::npos,
      "chemistry diagnostics should export the current chemistry contract revision");
  require_true(
      exported.find("\"chemistry_diagnostics_revision\":\"track5_batch4_chemistry_diagnostics_v1\"") !=
          std::string::npos,
      "chemistry diagnostics should export the current diagnostics revision");
  require_true(
      exported.find("\"chemistry_benchmark_gate_revision\":\"track5_batch4_chemistry_query_gates_v1\"") !=
          std::string::npos,
      "chemistry diagnostics should export the current benchmark-gate revision");
  require_true(
      exported.find("\"chemistry_namespace_summary\":\"chem.spectrum\"") != std::string::npos,
      "chemistry diagnostics should summarize the public chemistry namespace");
  require_true(
      exported.find("\"chemistry_spectra_subtype_summary\":\"chem.spectrum\"") !=
          std::string::npos,
      "chemistry diagnostics should summarize the chemistry subtype surface");
  require_true(
      exported.find("\"chemistry_spectra_source_reference_summary\":\"chem.spectrum_projection\"") !=
          std::string::npos,
      "chemistry diagnostics should summarize the chemistry source-reference surface");
  require_true(
      exported.find("\"chemistry_spectra_count\":5") != std::string::npos,
      "chemistry diagnostics should count live chemistry spectra");
  require_true(
      exported.find("\"chemistry_spectra_present_count\":2") != std::string::npos,
      "chemistry diagnostics should count present chemistry spectra");
  require_true(
      exported.find("\"chemistry_spectra_missing_count\":1") != std::string::npos,
      "chemistry diagnostics should count missing chemistry spectra");
  require_true(
      exported.find("\"chemistry_spectra_unresolved_count\":1") != std::string::npos,
      "chemistry diagnostics should count unresolved chemistry spectra");
  require_true(
      exported.find("\"chemistry_spectra_unsupported_count\":1") != std::string::npos,
      "chemistry diagnostics should count unsupported chemistry spectra");
  require_true(
      exported.find("\"chemistry_source_ref_count\":6") != std::string::npos,
      "chemistry diagnostics should count formal chemistry source refs");
  require_true(
      exported.find("\"chemistry_source_ref_resolved_count\":1") != std::string::npos,
      "chemistry diagnostics should count resolved chemistry source refs");
  require_true(
      exported.find("\"chemistry_source_ref_missing_count\":1") != std::string::npos,
      "chemistry diagnostics should count missing chemistry source refs");
  require_true(
      exported.find("\"chemistry_source_ref_stale_count\":1") != std::string::npos,
      "chemistry diagnostics should count stale chemistry source refs");
  require_true(
      exported.find("\"chemistry_source_ref_unresolved_count\":2") != std::string::npos,
      "chemistry diagnostics should count unresolved chemistry source refs");
  require_true(
      exported.find("\"chemistry_source_ref_unsupported_count\":1") != std::string::npos,
      "chemistry diagnostics should count unsupported chemistry source refs");
  require_true(
      exported.find("\"chemistry_unresolved_summary\":\"chemistry_spectra_and_source_refs\"") !=
          std::string::npos,
      "chemistry diagnostics should summarize unresolved chemistry anomalies");
  require_true(
      exported.find("\"chemistry_stale_summary\":\"chemistry_source_refs\"") !=
          std::string::npos,
      "chemistry diagnostics should summarize stale chemistry refs");
  require_true(
      exported.find("\"chemistry_unsupported_summary\":\"chemistry_spectra_and_source_refs\"") !=
          std::string::npos,
      "chemistry diagnostics should summarize unsupported chemistry anomalies");
  require_true(
      exported.find("\"chemistry_capability_track_status_summary\":\"chemistry_metadata=gated;chemistry_spectra=gated;chemistry_refs=gated;chemistry_gates=gated\"") !=
          std::string::npos,
      "chemistry diagnostics should export the current chemistry capability-track status summary");
  require_true(
      exported.find("\"last_chemistry_recount_reason\":\"\"") != std::string::npos,
      "chemistry diagnostics should leave last_chemistry_recount_reason empty before rebuild or watcher full rescan");
  require_true(
      exported.find("\"last_chemistry_recount_at_ns\":0") != std::string::npos,
      "chemistry diagnostics should leave last_chemistry_recount_at_ns zero before rebuild or watcher full rescan");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_chemistry_diagnostics_snapshot_tests() {
  test_export_diagnostics_reports_chemistry_contract_snapshot();
}
