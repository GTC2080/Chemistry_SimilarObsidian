// Reason: Keep Track 4 attachment-carrier domain-metadata coverage separate so
// the new substrate contract stays focused and easy to extend.

#include "kernel/c_api.h"

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
      entry->carrier_kind == KERNEL_DOMAIN_CARRIER_ATTACHMENT,
      std::string(context) + ": entry should preserve attachment carrier kind");
  require_true(
      entry->carrier_key != nullptr,
      std::string(context) + ": entry should preserve carrier key");
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

void test_attachment_domain_metadata_surface_projects_registered_generic_entries() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "assets");
  write_file_bytes(vault / "assets" / "diagram.png", "png-bytes");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string note =
      "# Domain Attachment\n"
      "![Diagram](assets/diagram.png)\n"
      "[Chem](assets/missing.sdf)\n";
  expect_ok(kernel_write_note(
      handle,
      "domain-attachment.md",
      note.data(),
      note.size(),
      nullptr,
      &metadata,
      &disposition));

  kernel_domain_metadata_list entries{};
  expect_ok(kernel_query_attachment_domain_metadata(
      handle,
      "assets/diagram.png",
      static_cast<size_t>(-1),
      &entries));
  require_true(entries.count == 3, "attachment domain metadata should expose exactly three generic entries");
  require_true(
      std::string(entries.entries[0].key_name) == "carrier_surface" &&
          std::string(entries.entries[1].key_name) == "coarse_kind" &&
          std::string(entries.entries[2].key_name) == "presence",
      "attachment domain metadata should sort entries by key_name");
  require_token_entry(
      entries,
      "carrier_surface",
      "attachment",
      "attachment carrier surface");
  require_token_entry(
      entries,
      "coarse_kind",
      "image_like",
      "attachment coarse kind");
  require_token_entry(
      entries,
      "presence",
      "present",
      "attachment presence");
  require_true(
      std::string(entries.entries[0].carrier_key) == "assets/diagram.png",
      "attachment domain metadata should preserve the normalized live public key");
  kernel_free_domain_metadata_list(&entries);

  expect_ok(kernel_query_attachment_domain_metadata(handle, "assets\\diagram.png", 2, &entries));
  require_true(
      entries.count == 2,
      "attachment domain metadata limit should trim the sorted entry list");
  require_true(
      std::string(entries.entries[0].carrier_key) == "assets/diagram.png",
      "attachment domain metadata should normalize backslash input");
  kernel_free_domain_metadata_list(&entries);

  expect_ok(kernel_query_attachment_domain_metadata(
      handle,
      "assets/missing.sdf",
      static_cast<size_t>(-1),
      &entries));
  require_token_entry(
      entries,
      "coarse_kind",
      "chem_like",
      "missing chem attachment coarse kind");
  require_token_entry(
      entries,
      "presence",
      "missing",
      "missing chem attachment presence");
  kernel_free_domain_metadata_list(&entries);

  write_file_bytes(vault / "assets" / "missing.sdf", "mol-bytes");
  expect_ok(kernel_rebuild_index(handle));
  expect_ok(kernel_query_attachment_domain_metadata(
      handle,
      "assets/missing.sdf",
      static_cast<size_t>(-1),
      &entries));
  require_token_entry(
      entries,
      "presence",
      "present",
      "attachment domain metadata should follow rebuild truth after missing file appears");
  kernel_free_domain_metadata_list(&entries);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_attachment_domain_metadata_surface_rejects_invalid_and_non_live_paths() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "assets");
  write_file_bytes(vault / "assets" / "unreferenced.png", "png-bytes");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_domain_metadata_list entries{};
  require_true(
      kernel_query_attachment_domain_metadata(handle, "", static_cast<size_t>(-1), &entries).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "attachment domain metadata should reject empty attachment path");
  require_true(
      entries.entries == nullptr && entries.count == 0,
      "attachment domain metadata should clear stale output on invalid input");

  require_true(
      kernel_query_attachment_domain_metadata(
          handle,
          "assets/unreferenced.png",
          static_cast<size_t>(-1),
          &entries)
              .code == KERNEL_ERROR_NOT_FOUND,
      "attachment domain metadata should reject unreferenced disk files");
  require_true(
      entries.entries == nullptr && entries.count == 0,
      "attachment domain metadata should clear stale output on NOT_FOUND");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_domain_surface_attachment_tests() {
  test_attachment_domain_metadata_surface_projects_registered_generic_entries();
  test_attachment_domain_metadata_surface_rejects_invalid_and_non_live_paths();
}
