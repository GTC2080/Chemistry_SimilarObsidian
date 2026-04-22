// Reason: Keep PDF recount diagnostics separate from broader snapshot coverage
// so Batch 4 rebuild/full-rescan supportability stays easy to reason about.

#include "kernel/c_api.h"

#include "api/kernel_api_pdf_diagnostics_suites.h"
#include "api/kernel_api_test_support.h"
#include "core/kernel_internal.h"
#include "core/kernel_pdf_query_shared.h"
#include "support/test_support.h"
#include "watcher/session.h"

#include <filesystem>
#include <string>
#include <vector>

namespace {

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

void test_export_diagnostics_reports_last_pdf_recount_after_rebuild() {
  const auto vault = make_temp_vault();
  const auto export_path = vault / "pdf-recount-rebuild.json";
  std::filesystem::create_directories(vault / "assets");
  write_file_bytes(vault / "assets" / "rebuild.pdf", make_pdf_bytes("Rebuild PDF Text"));

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "pdf rebuild recount diagnostics test should start from READY");

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "pdf-rebuild.md",
      "# PDF Rebuild\n[PDF](assets/rebuild.pdf)\n",
      std::string_view("# PDF Rebuild\n[PDF](assets/rebuild.pdf)\n").size(),
      nullptr,
      &metadata,
      &disposition));

  const auto anchor_records = query_pdf_anchors(handle, "assets/rebuild.pdf");
  require_true(anchor_records.size() == 1, "pdf rebuild recount test should materialize one anchor");

  const std::string source_note =
      "# PDF Source Rebuild\n"
      "[Source](assets/rebuild.pdf#anchor=" + anchor_records[0].anchor_serialized + ")\n";
  expect_ok(kernel_write_note(
      handle,
      "pdf-rebuild-source.md",
      source_note.data(),
      source_note.size(),
      nullptr,
      &metadata,
      &disposition));

  expect_ok(kernel_rebuild_index(handle));
  expect_ok(kernel_export_diagnostics(handle, export_path.string().c_str()));
  const std::string exported = read_file_text(export_path);

  require_true(
      exported.find("\"last_pdf_recount_reason\":\"rebuild\"") != std::string::npos,
      "pdf diagnostics should record rebuild as the last pdf recount reason after rebuild");
  require_true(
      exported.find("\"last_pdf_recount_at_ns\":0") == std::string::npos,
      "pdf diagnostics should export a non-zero pdf recount timestamp after rebuild");
  require_true(
      exported.find("\"pdf_live_anchor_count\":1") != std::string::npos,
      "pdf diagnostics should keep anchor counts after rebuild");
  require_true(
      exported.find("\"pdf_source_ref_count\":1") != std::string::npos,
      "pdf diagnostics should keep source-ref counts after rebuild");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_export_diagnostics_reports_last_pdf_recount_after_watcher_full_rescan() {
  const auto vault = make_temp_vault();
  const auto export_path = vault / "pdf-recount-watcher.json";
  std::filesystem::create_directories(vault / "assets");
  write_file_bytes(vault / "assets" / "watcher.pdf", make_pdf_bytes("Original Watcher Text"));

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "pdf watcher recount diagnostics test should start from READY");

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "pdf-watcher.md",
      "# PDF Watcher\n[PDF](assets/watcher.pdf)\n",
      std::string_view("# PDF Watcher\n[PDF](assets/watcher.pdf)\n").size(),
      nullptr,
      &metadata,
      &disposition));

  const auto anchor_records = query_pdf_anchors(handle, "assets/watcher.pdf");
  require_true(anchor_records.size() == 1, "pdf watcher recount test should materialize one anchor");

  const std::string source_note =
      "# PDF Watcher Source\n"
      "[Source](assets/watcher.pdf#anchor=" + anchor_records[0].anchor_serialized + ")\n";
  expect_ok(kernel_write_note(
      handle,
      "pdf-watcher-source.md",
      source_note.data(),
      source_note.size(),
      nullptr,
      &metadata,
      &disposition));

  write_file_bytes(vault / "assets" / "watcher.pdf", make_pdf_bytes("Changed Watcher Text"));
  kernel::watcher::inject_next_poll_overflow(handle->watcher_session);
  require_eventually(
      [&]() {
        kernel_pdf_source_refs refs{};
        const kernel_status refs_status =
            kernel_query_note_pdf_source_refs(handle, "pdf-watcher-source.md", 4, &refs);
        if (refs_status.code != KERNEL_OK) {
          kernel_free_pdf_source_refs(&refs);
          return false;
        }

        const bool stale_ref_seen =
            refs.count == 1 && refs.refs[0].state == KERNEL_PDF_REF_STALE;
        kernel_free_pdf_source_refs(&refs);

        std::lock_guard runtime_lock(handle->runtime_mutex);
        return stale_ref_seen &&
               handle->runtime.last_pdf_recount.reason == "watcher_full_rescan" &&
               handle->runtime.last_pdf_recount.at_ns != 0;
      },
      "overflow-driven full rescan should stale the pdf source ref and record a pdf recount");

  expect_ok(kernel_export_diagnostics(handle, export_path.string().c_str()));
  const std::string exported = read_file_text(export_path);
  require_true(
      exported.find("\"last_pdf_recount_reason\":\"watcher_full_rescan\"") != std::string::npos,
      "pdf diagnostics should report watcher_full_rescan as the last pdf recount reason after overflow");
  require_true(
      exported.find("\"last_pdf_recount_at_ns\":0") == std::string::npos,
      "pdf diagnostics should export a non-zero pdf recount timestamp after overflow");
  require_true(
      exported.find("\"pdf_source_ref_stale_count\":1") != std::string::npos,
      "pdf diagnostics should report one stale pdf source ref after the overflow-driven rescan");
  require_true(
      exported.find("\"pdf_reference_anomaly_summary\":\"stale_pdf_references\"") !=
          std::string::npos,
      "pdf diagnostics should summarize stale pdf references after overflow-driven rescan");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_pdf_diagnostics_recount_tests() {
  test_export_diagnostics_reports_last_pdf_recount_after_rebuild();
  test_export_diagnostics_reports_last_pdf_recount_after_watcher_full_rescan();
}
