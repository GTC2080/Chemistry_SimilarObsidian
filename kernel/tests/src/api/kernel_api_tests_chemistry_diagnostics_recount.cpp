// Reason: Keep Track 5 chemistry recount diagnostics separate so rebuild and
// overflow-driven supportability stay easy to reason about.

#include "kernel/c_api.h"

#include "api/kernel_api_chemistry_diagnostics_suites.h"
#include "api/kernel_api_test_support.h"
#include "chemistry/chemistry_spectrum_metadata.h"
#include "chemistry/chemistry_spectrum_selector.h"
#include "core/kernel_internal.h"
#include "support/test_support.h"
#include "vault/revision.h"
#include "watcher/session.h"

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
      "chemistry recount selector fixture should parse");

  kernel::chemistry::ParsedChemSpectrumSelector selector;
  selector.kind = kernel::chemistry::ChemSpectrumSelectorKind::WholeSpectrum;
  selector.chemistry_selector_basis_revision =
      kernel::chemistry::build_chemistry_selector_basis_revision(
          parsed.metadata.attachment_content_revision,
          kernel::chemistry::build_normalized_spectrum_basis(parsed.metadata));
  return kernel::chemistry::serialize_chem_spectrum_selector(selector);
}

bool chemistry_ref_is_stale(
    kernel_handle* handle,
    const char* note_rel_path) {
  kernel_chem_spectrum_source_refs refs{};
  const kernel_status status =
      kernel_query_note_chem_spectrum_refs(handle, note_rel_path, 4, &refs);
  if (status.code != KERNEL_OK) {
    kernel_free_chem_spectrum_source_refs(&refs);
    return false;
  }

  const bool stale =
      refs.count == 1 && refs.refs[0].state == KERNEL_DOMAIN_REF_STALE;
  kernel_free_chem_spectrum_source_refs(&refs);
  return stale;
}

void test_export_diagnostics_reports_last_chemistry_recount_after_rebuild() {
  const auto vault = make_temp_vault();
  const auto export_path = vault / "chemistry-recount-rebuild.json";
  std::filesystem::create_directories(vault / "spectra");
  write_file_bytes(
      vault / "spectra" / "rebuild.jdx",
      make_jcamp_bytes("Rebuild", "NMR SPECTRUM", "PPM", "INTENSITY", 4));

  const std::string selector =
      build_whole_selector_for_file(vault, "spectra/rebuild.jdx");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "chemistry rebuild recount test should start from READY");

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string note =
      "# Chemistry Rebuild\n"
      "[Spectrum](spectra/rebuild.jdx#chemsel=" + selector + ")\n";
  expect_ok(kernel_write_note(
      handle,
      "chemistry-rebuild.md",
      note.data(),
      note.size(),
      nullptr,
      &metadata,
      &disposition));

  expect_ok(kernel_rebuild_index(handle));
  expect_ok(kernel_export_diagnostics(handle, export_path.string().c_str()));
  const std::string exported = read_file_text(export_path);

  require_true(
      exported.find("\"last_chemistry_recount_reason\":\"rebuild\"") !=
          std::string::npos,
      "chemistry diagnostics should report rebuild as the last chemistry recount reason after rebuild");
  require_true(
      exported.find("\"last_chemistry_recount_at_ns\":0") == std::string::npos,
      "chemistry diagnostics should export a non-zero chemistry recount timestamp after rebuild");
  require_true(
      exported.find("\"chemistry_spectra_count\":1") != std::string::npos,
      "chemistry diagnostics should preserve spectrum counts after rebuild");
  require_true(
      exported.find("\"chemistry_source_ref_count\":1") != std::string::npos,
      "chemistry diagnostics should preserve source-ref counts after rebuild");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_export_diagnostics_reports_last_chemistry_recount_after_watcher_full_rescan() {
  const auto vault = make_temp_vault();
  const auto export_path = vault / "chemistry-recount-watcher.json";
  std::filesystem::create_directories(vault / "spectra");
  write_file_bytes(
      vault / "spectra" / "watcher.jdx",
      make_jcamp_bytes("Watcher", "NMR SPECTRUM", "PPM", "INTENSITY", 4));

  const std::string selector =
      build_whole_selector_for_file(vault, "spectra/watcher.jdx");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "chemistry watcher recount test should start from READY");

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string note =
      "# Chemistry Watcher\n"
      "[Spectrum](spectra/watcher.jdx#chemsel=" + selector + ")\n";
  expect_ok(kernel_write_note(
      handle,
      "chemistry-watcher.md",
      note.data(),
      note.size(),
      nullptr,
      &metadata,
      &disposition));

  write_file_bytes(
      vault / "spectra" / "watcher.jdx",
      make_jcamp_bytes("Watcher Changed", "NMR SPECTRUM", "PPM", "INTENSITY", 6));
  kernel::watcher::inject_next_poll_overflow(handle->watcher_session);
  require_eventually(
      [&]() {
        if (!chemistry_ref_is_stale(handle, "chemistry-watcher.md")) {
          return false;
        }

        std::lock_guard runtime_lock(handle->runtime_mutex);
        return handle->runtime.last_chemistry_recount.reason == "watcher_full_rescan" &&
               handle->runtime.last_chemistry_recount.at_ns != 0;
      },
      "overflow-driven full rescan should stale the chemistry ref and record a chemistry recount");

  expect_ok(kernel_export_diagnostics(handle, export_path.string().c_str()));
  const std::string exported = read_file_text(export_path);
  require_true(
      exported.find("\"last_chemistry_recount_reason\":\"watcher_full_rescan\"") !=
          std::string::npos,
      "chemistry diagnostics should report watcher_full_rescan as the last chemistry recount reason after overflow");
  require_true(
      exported.find("\"last_chemistry_recount_at_ns\":0") == std::string::npos,
      "chemistry diagnostics should export a non-zero chemistry recount timestamp after overflow");
  require_true(
      exported.find("\"chemistry_source_ref_stale_count\":1") != std::string::npos,
      "chemistry diagnostics should report one stale chemistry source ref after overflow-driven rescan");
  require_true(
      exported.find("\"chemistry_stale_summary\":\"chemistry_source_refs\"") !=
          std::string::npos,
      "chemistry diagnostics should summarize stale chemistry refs after overflow-driven rescan");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_chemistry_diagnostics_recount_tests() {
  test_export_diagnostics_reports_last_chemistry_recount_after_rebuild();
  test_export_diagnostics_reports_last_chemistry_recount_after_watcher_full_rescan();
}
