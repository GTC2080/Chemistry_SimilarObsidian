// Reason: Keep Track 4 PDF-carrier domain-metadata coverage separate so the
// new substrate contract stays out of the existing PDF surface suites.

#include "kernel/c_api.h"

#include "api/kernel_api_pdf_test_helpers.h"
#include "api/kernel_api_domain_surface_suites.h"
#include "support/test_support.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace {

const kernel_domain_metadata_entry* find_entry(
    const kernel_domain_metadata_list& entries,
    const char* key_name) {
  for (size_t index = 0; index < entries.count; ++index) {
    if (entries.entries[index].key_name != nullptr &&
        std::string(entries.entries[index].key_name) == key_name) {
      return &entries.entries[index];
    }
  }
  return nullptr;
}

void require_token_entry(
    const kernel_domain_metadata_list& entries,
    const char* key_name,
    std::string_view expected_value,
    std::string_view context) {
  const auto* entry = find_entry(entries, key_name);
  require_true(entry != nullptr, std::string(context) + ": expected registered token entry");
  require_true(
      entry->carrier_kind == KERNEL_DOMAIN_CARRIER_PDF,
      std::string(context) + ": entry should preserve PDF carrier kind");
  require_true(
      entry->namespace_name != nullptr && std::string(entry->namespace_name) == "generic",
      std::string(context) + ": entry should preserve the generic namespace");
  require_true(
      entry->public_schema_revision == 1,
      std::string(context) + ": entry should preserve the generic namespace revision");
  require_true(
      entry->value_kind == KERNEL_DOMAIN_VALUE_TOKEN,
      std::string(context) + ": entry should preserve token value kind");
  require_true(
      entry->string_value != nullptr && std::string(entry->string_value) == expected_value,
      std::string(context) + ": entry should preserve the expected token value");
}

void test_pdf_domain_metadata_surface_projects_registered_generic_entries() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "assets");
  write_file_bytes(
      vault / "assets" / "ready.pdf",
      make_metadata_pdf_bytes(2, true, true, "/Title (Ready PDF) "));
  write_file_bytes(vault / "assets" / "plain.png", "png-bytes");
  write_file_bytes(
      vault / "assets" / "unreferenced.pdf",
      make_metadata_pdf_bytes(1, false, true, "/Title (Unreferenced PDF) "));

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string note =
      "# Domain PDF\n"
      "[Ready](assets/ready.pdf)\n"
      "[Missing](assets/missing.pdf)\n";
  expect_ok(kernel_write_note(
      handle,
      "domain-pdf.md",
      note.data(),
      note.size(),
      nullptr,
      &metadata,
      &disposition));

  kernel_domain_metadata_list entries{};
  expect_ok(kernel_query_pdf_domain_metadata(
      handle,
      "assets/ready.pdf",
      static_cast<size_t>(-1),
      &entries));
  require_true(entries.count == 7, "pdf domain metadata should expose exactly seven generic entries");
  require_true(
      std::string(entries.entries[0].key_name) == "carrier_surface" &&
          std::string(entries.entries[1].key_name) == "doc_title_state" &&
          std::string(entries.entries[2].key_name) == "has_outline" &&
          std::string(entries.entries[3].key_name) == "metadata_state" &&
          std::string(entries.entries[4].key_name) == "page_count" &&
          std::string(entries.entries[5].key_name) == "presence" &&
          std::string(entries.entries[6].key_name) == "text_layer_state",
      "pdf domain metadata should sort entries by key_name");
  require_true(
      std::string(entries.entries[0].carrier_key) == "assets/ready.pdf",
      "pdf domain metadata should preserve the normalized live public key");
  require_token_entry(entries, "carrier_surface", "pdf", "pdf carrier surface");
  require_token_entry(entries, "doc_title_state", "available", "pdf doc-title state");
  require_token_entry(entries, "metadata_state", "ready", "pdf metadata state");
  require_token_entry(entries, "presence", "present", "pdf presence");
  require_token_entry(entries, "text_layer_state", "present", "pdf text-layer state");
  const auto* page_count = find_entry(entries, "page_count");
  require_true(page_count != nullptr, "pdf domain metadata should expose page_count");
  require_true(
      page_count->value_kind == KERNEL_DOMAIN_VALUE_UINT64 && page_count->uint64_value == 2,
      "pdf domain metadata should expose uint64 page_count");
  const auto* has_outline = find_entry(entries, "has_outline");
  require_true(has_outline != nullptr, "pdf domain metadata should expose has_outline");
  require_true(
      has_outline->value_kind == KERNEL_DOMAIN_VALUE_BOOL && has_outline->bool_value == 1,
      "pdf domain metadata should expose bool has_outline");
  kernel_free_domain_metadata_list(&entries);

  expect_ok(kernel_query_pdf_domain_metadata(handle, "assets\\ready.pdf", 3, &entries));
  require_true(entries.count == 3, "pdf domain metadata limit should trim the sorted entry list");
  require_true(
      std::string(entries.entries[0].carrier_key) == "assets/ready.pdf",
      "pdf domain metadata should normalize backslash input");
  kernel_free_domain_metadata_list(&entries);

  expect_ok(kernel_query_pdf_domain_metadata(
      handle,
      "assets/missing.pdf",
      static_cast<size_t>(-1),
      &entries));
  require_token_entry(entries, "presence", "missing", "missing pdf presence");
  require_token_entry(entries, "metadata_state", "unavailable", "missing pdf metadata state");
  page_count = find_entry(entries, "page_count");
  require_true(
      page_count != nullptr && page_count->uint64_value == 0,
      "missing pdf domain metadata should preserve zero page_count when no extraction exists");
  kernel_free_domain_metadata_list(&entries);

  write_file_bytes(
      vault / "assets" / "ready.pdf",
      make_metadata_pdf_bytes(3, false, false, "/Title (Changed Ready PDF) "));
  expect_ok(kernel_rebuild_index(handle));
  expect_ok(kernel_query_pdf_domain_metadata(
      handle,
      "assets/ready.pdf",
      static_cast<size_t>(-1),
      &entries));
  require_token_entry(entries, "metadata_state", "ready", "pdf metadata state after rebuild");
  require_token_entry(entries, "text_layer_state", "absent", "pdf text-layer state after rebuild");
  page_count = find_entry(entries, "page_count");
  require_true(
      page_count != nullptr && page_count->uint64_value == 3,
      "pdf domain metadata should follow rebuild truth for page_count");
  has_outline = find_entry(entries, "has_outline");
  require_true(
      has_outline != nullptr && has_outline->bool_value == 0,
      "pdf domain metadata should follow rebuild truth for outline presence");
  kernel_free_domain_metadata_list(&entries);

  require_true(
      kernel_query_pdf_domain_metadata(handle, "assets/plain.png", static_cast<size_t>(-1), &entries)
              .code == KERNEL_ERROR_NOT_FOUND,
      "pdf domain metadata should reject non-pdf live attachments");
  require_true(
      entries.entries == nullptr && entries.count == 0,
      "pdf domain metadata should clear stale output on non-pdf NOT_FOUND");
  require_true(
      kernel_query_pdf_domain_metadata(
          handle,
          "assets/unreferenced.pdf",
          static_cast<size_t>(-1),
          &entries)
              .code == KERNEL_ERROR_NOT_FOUND,
      "pdf domain metadata should reject unreferenced disk pdfs");
  require_true(
      entries.entries == nullptr && entries.count == 0,
      "pdf domain metadata should clear stale output on unreferenced pdf NOT_FOUND");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_pdf_domain_metadata_surface_rejects_invalid_arguments() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_domain_metadata_list entries{};
  require_true(
      kernel_query_pdf_domain_metadata(handle, "", static_cast<size_t>(-1), &entries).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "pdf domain metadata should reject empty attachment path");
  require_true(
      entries.entries == nullptr && entries.count == 0,
      "pdf domain metadata should clear stale output on invalid input");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_domain_surface_pdf_tests() {
  test_pdf_domain_metadata_surface_projects_registered_generic_entries();
  test_pdf_domain_metadata_surface_rejects_invalid_arguments();
}
