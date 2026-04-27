// Reason: This file verifies the host-facing note catalog surface used by app shells.

#include "kernel/c_api.h"

#include "api/kernel_api_core_base_suites.h"
#include "api/kernel_api_test_support.h"
#include "support/test_support.h"

#include <filesystem>
#include <string>

namespace {

void write_note(kernel_handle* handle, const char* rel_path, const std::string& content) {
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      rel_path,
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));
  require_true(disposition == KERNEL_WRITE_WRITTEN, "test note should be written");
}

void test_query_notes_returns_sorted_live_catalog() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string beta = "# Beta Title\nbody";
  const std::string alpha = "# Alpha Title\nbody";
  write_note(handle, "zeta/beta.md", beta);
  write_note(handle, "alpha.md", alpha);

  kernel_note_list notes{};
  expect_ok(kernel_query_notes(handle, 16, &notes));
  require_true(notes.count == 2, "note catalog should include two active notes");
  require_true(std::string(notes.notes[0].rel_path) == "alpha.md", "note catalog should sort by rel_path");
  require_true(std::string(notes.notes[0].title) == "Alpha Title", "note catalog should expose parser title");
  require_true(notes.notes[0].file_size == alpha.size(), "note catalog should expose file size");
  require_true(notes.notes[0].mtime_ns != 0, "note catalog should expose mtime");
  require_true(notes.notes[0].content_revision[0] != '\0', "note catalog should expose content revision");
  require_true(std::string(notes.notes[1].rel_path) == "zeta/beta.md", "note catalog should preserve nested rel_path");
  require_true(std::string(notes.notes[1].title) == "Beta Title", "note catalog should expose nested note title");
  kernel_free_note_list(&notes);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_query_notes_limit_is_applied() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  write_note(handle, "b.md", "# B\n");
  write_note(handle, "a.md", "# A\n");

  kernel_note_list notes{};
  expect_ok(kernel_query_notes(handle, 1, &notes));
  require_true(notes.count == 1, "note catalog limit should cap results");
  require_true(std::string(notes.notes[0].rel_path) == "a.md", "limited note catalog should still sort first");
  kernel_free_note_list(&notes);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_note_catalog_default_limit_is_kernel_owned() {
  size_t limit = 0;
  expect_ok(kernel_get_note_catalog_default_limit(&limit));
  require_true(limit == 100000, "note catalog default limit should come from kernel");
  require_true(
      kernel_get_note_catalog_default_limit(nullptr).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "note catalog default limit should reject null output");
}

void test_vault_scan_default_limit_is_kernel_owned() {
  size_t limit = 0;
  expect_ok(kernel_get_vault_scan_default_limit(&limit));
  require_true(limit == 4096, "vault scan default limit should come from kernel");
  require_true(
      kernel_get_vault_scan_default_limit(nullptr).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "vault scan default limit should reject null output");
}

void test_query_notes_filtered_ignores_only_matching_roots() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  write_note(handle, "node_modules/pkg.md", "# Package\n");
  write_note(handle, "node_modules.md", "# Root File\n");
  write_note(handle, "lab/a.md", "# A\n");

  kernel_note_list notes{};
  expect_ok(kernel_query_notes_filtered(handle, 16, " node_modules/ ", &notes));
  require_true(notes.count == 2, "filtered note catalog should remove only exact ignored roots");
  require_true(std::string(notes.notes[0].rel_path) == "lab/a.md", "filtered note catalog should keep lab");
  require_true(
      std::string(notes.notes[1].rel_path) == "node_modules.md",
      "filtered note catalog should keep similarly named root files");
  kernel_free_note_list(&notes);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_query_notes_requires_valid_arguments() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_list notes{};
  require_true(
      kernel_query_notes(nullptr, 1, &notes).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "note catalog should require handle");
  require_true(
      kernel_query_notes(handle, 0, &notes).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "note catalog should reject zero limit");
  require_true(
      kernel_query_notes(handle, 1, nullptr).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "note catalog should require output pointer");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_kernel_api_core_note_catalog_contract_tests() {
  test_query_notes_returns_sorted_live_catalog();
  test_query_notes_limit_is_applied();
  test_note_catalog_default_limit_is_kernel_owned();
  test_vault_scan_default_limit_is_kernel_owned();
  test_query_notes_filtered_ignores_only_matching_roots();
  test_query_notes_requires_valid_arguments();
}
