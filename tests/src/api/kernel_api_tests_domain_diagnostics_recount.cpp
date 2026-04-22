// Reason: Keep Track 4 domain recount diagnostics separate so rebuild/full-
// rescan supportability stays easy to reason about.

#include "kernel/c_api.h"

#include "api/kernel_api_domain_diagnostics_suites.h"
#include "api/kernel_api_pdf_test_helpers.h"
#include "api/kernel_api_test_support.h"
#include "core/kernel_internal.h"
#include "support/test_support.h"
#include "watcher/session.h"

#include <filesystem>
#include <string>
#include <vector>

namespace {

void test_export_diagnostics_reports_last_domain_recount_after_rebuild() {
  const auto vault = make_temp_vault();
  const auto export_path = vault / "domain-recount-rebuild.json";
  std::filesystem::create_directories(vault / "assets");
  write_file_bytes(vault / "assets" / "rebuild.pdf", make_text_pdf_bytes("Rebuild Domain PDF Text"));

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "domain rebuild recount diagnostics test should start from READY");

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "domain-rebuild.md",
      "# Domain Rebuild\n[PDF](assets/rebuild.pdf)\n",
      std::string_view("# Domain Rebuild\n[PDF](assets/rebuild.pdf)\n").size(),
      nullptr,
      &metadata,
      &disposition));

  const auto anchor_records = query_pdf_anchor_records(handle, "assets/rebuild.pdf");
  require_true(anchor_records.size() == 1, "domain rebuild recount test should materialize one anchor");
  const std::string source_note =
      "# Domain Source Rebuild\n"
      "[Source](assets/rebuild.pdf#anchor=" + anchor_records[0].anchor_serialized + ")\n";
  expect_ok(kernel_write_note(
      handle,
      "domain-rebuild-source.md",
      source_note.data(),
      source_note.size(),
      nullptr,
      &metadata,
      &disposition));

  expect_ok(kernel_rebuild_index(handle));
  expect_ok(kernel_export_diagnostics(handle, export_path.string().c_str()));
  const std::string exported = read_file_text(export_path);

  require_true(
      exported.find("\"last_domain_recount_reason\":\"rebuild\"") != std::string::npos,
      "domain diagnostics should report rebuild as the last domain recount reason after rebuild");
  require_true(
      exported.find("\"last_domain_recount_at_ns\":0") == std::string::npos,
      "domain diagnostics should export a non-zero domain recount timestamp after rebuild");
  require_true(
      exported.find("\"domain_source_ref_count\":1") != std::string::npos,
      "domain diagnostics should preserve generic source-ref counts after rebuild");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_export_diagnostics_reports_last_domain_recount_after_watcher_full_rescan() {
  const auto vault = make_temp_vault();
  const auto export_path = vault / "domain-recount-watcher.json";
  std::filesystem::create_directories(vault / "assets");
  write_file_bytes(
      vault / "assets" / "watcher.pdf",
      make_text_pdf_bytes("Original Domain Watcher Text"));

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "domain watcher recount diagnostics test should start from READY");

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "domain-watcher.md",
      "# Domain Watcher\n[PDF](assets/watcher.pdf)\n",
      std::string_view("# Domain Watcher\n[PDF](assets/watcher.pdf)\n").size(),
      nullptr,
      &metadata,
      &disposition));

  const auto anchor_records = query_pdf_anchor_records(handle, "assets/watcher.pdf");
  require_true(anchor_records.size() == 1, "domain watcher recount test should materialize one anchor");
  const std::string source_note =
      "# Domain Watcher Source\n"
      "[Source](assets/watcher.pdf#anchor=" + anchor_records[0].anchor_serialized + ")\n";
  expect_ok(kernel_write_note(
      handle,
      "domain-watcher-source.md",
      source_note.data(),
      source_note.size(),
      nullptr,
      &metadata,
      &disposition));

  write_file_bytes(vault / "assets" / "watcher.pdf", make_text_pdf_bytes("Changed Domain Watcher Text"));
  kernel::watcher::inject_next_poll_overflow(handle->watcher_session);
  require_eventually(
      [&]() {
        kernel_domain_source_refs refs{};
        const kernel_status refs_status =
            kernel_query_note_domain_source_refs(handle, "domain-watcher-source.md", 4, &refs);
        if (refs_status.code != KERNEL_OK) {
          kernel_free_domain_source_refs(&refs);
          return false;
        }

        const bool stale_ref_seen =
            refs.count == 1 && refs.refs[0].state == KERNEL_DOMAIN_REF_STALE;
        kernel_free_domain_source_refs(&refs);

        std::lock_guard runtime_lock(handle->runtime_mutex);
        return stale_ref_seen &&
               handle->runtime.last_domain_recount.reason == "watcher_full_rescan" &&
               handle->runtime.last_domain_recount.at_ns != 0;
      },
      "overflow-driven full rescan should stale the projected domain ref and record a domain recount");

  expect_ok(kernel_export_diagnostics(handle, export_path.string().c_str()));
  const std::string exported = read_file_text(export_path);
  require_true(
      exported.find("\"last_domain_recount_reason\":\"watcher_full_rescan\"") != std::string::npos,
      "domain diagnostics should report watcher_full_rescan as the last domain recount reason after overflow");
  require_true(
      exported.find("\"last_domain_recount_at_ns\":0") == std::string::npos,
      "domain diagnostics should export a non-zero domain recount timestamp after overflow");
  require_true(
      exported.find("\"domain_source_ref_stale_count\":1") != std::string::npos,
      "domain diagnostics should report one stale projected domain source ref after overflow-driven rescan");
  require_true(
      exported.find("\"domain_stale_summary\":\"domain_source_refs\"") != std::string::npos,
      "domain diagnostics should summarize stale projected domain refs after overflow-driven rescan");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_domain_diagnostics_recount_tests() {
  test_export_diagnostics_reports_last_domain_recount_after_rebuild();
  test_export_diagnostics_reports_last_domain_recount_after_watcher_full_rescan();
}
