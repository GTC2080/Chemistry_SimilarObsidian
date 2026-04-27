// Reason: This file verifies host-facing vault entry mutations stay aligned with the note catalog.

#include "kernel/c_api.h"

#include "api/kernel_api_core_base_suites.h"
#include "support/test_support.h"

#include <filesystem>
#include <string>

namespace {

void close_and_cleanup(kernel_handle* handle, const std::filesystem::path& vault) {
  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

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
  require_true(disposition == KERNEL_WRITE_WRITTEN, "vault entry test note should be written");
}

kernel_note_list query_notes(kernel_handle* handle) {
  kernel_note_list notes{};
  expect_ok(kernel_query_notes(handle, 16, &notes));
  return notes;
}

void test_create_folder_creates_single_directory() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  expect_ok(kernel_create_folder(handle, "folder"));
  require_true(std::filesystem::is_directory(vault / "folder"), "create folder should create directory");
  require_true(
      kernel_create_folder(handle, "folder").code == KERNEL_ERROR_CONFLICT,
      "create folder should reject duplicate target");
  require_true(
      kernel_create_folder(handle, "../escape").code == KERNEL_ERROR_INVALID_ARGUMENT,
      "create folder should reject escape path");

  close_and_cleanup(handle, vault);
}

void test_rename_note_updates_catalog_and_preserves_content() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  write_note(handle, "old.md", "# Old\nbody");
  expect_ok(kernel_rename_entry(handle, "old.md", "new"));

  require_true(!std::filesystem::exists(vault / "old.md"), "rename should remove old file");
  require_true(std::filesystem::exists(vault / "new.md"), "rename should preserve markdown extension");

  kernel_note_list notes = query_notes(handle);
  require_true(notes.count == 1, "rename should preserve one live note");
  require_true(std::string(notes.notes[0].rel_path) == "new.md", "rename should update catalog rel_path");
  require_true(std::string(notes.notes[0].title) == "Old", "rename should preserve note content metadata");
  kernel_free_note_list(&notes);

  close_and_cleanup(handle, vault);
}

void test_delete_note_removes_catalog_entry() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  write_note(handle, "delete-me.md", "# Delete me\n");
  expect_ok(kernel_delete_entry(handle, "delete-me.md"));

  require_true(!std::filesystem::exists(vault / "delete-me.md"), "delete should remove note file");
  kernel_note_list notes = query_notes(handle);
  require_true(notes.count == 0, "delete should remove note from live catalog");
  kernel_free_note_list(&notes);

  close_and_cleanup(handle, vault);
}

void test_move_note_updates_catalog() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  expect_ok(kernel_create_folder(handle, "target"));
  write_note(handle, "move-me.md", "# Move me\n");
  expect_ok(kernel_move_entry(handle, "move-me.md", "target"));

  require_true(!std::filesystem::exists(vault / "move-me.md"), "move should remove source file");
  require_true(std::filesystem::exists(vault / "target" / "move-me.md"), "move should place note in folder");

  kernel_note_list notes = query_notes(handle);
  require_true(notes.count == 1, "move should preserve one live note");
  require_true(
      std::string(notes.notes[0].rel_path) == "target/move-me.md",
      "move should update catalog rel_path");
  kernel_free_note_list(&notes);

  close_and_cleanup(handle, vault);
}

void test_filter_changed_markdown_paths_normalizes_filters_and_deduplicates() {
  kernel_path_list paths{};
  expect_ok(kernel_filter_changed_markdown_paths(
      " Folder\\Note.md \n"
      "Folder/Note.md\n"
      "Folder/Note.txt\n"
      "\n"
      "Folder/Sub.MD\n",
      &paths));

  require_true(paths.count == 2, "changed path filter should keep only unique markdown paths");
  require_true(
      std::string(paths.paths[0]) == "Folder/Note.md",
      "changed path filter should trim and normalize backslashes");
  require_true(
      std::string(paths.paths[1]) == "Folder/Sub.MD",
      "changed path filter should preserve original path case");
  kernel_free_path_list(&paths);
  require_true(paths.paths == nullptr && paths.count == 0, "changed path free should reset output");
  kernel_free_path_list(&paths);
  require_true(
      kernel_filter_changed_markdown_paths("note.md", nullptr).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "changed path filter should require output pointer");
}

void test_filter_supported_vault_paths_normalizes_filters_and_deduplicates() {
  kernel_path_list paths{};
  expect_ok(kernel_filter_supported_vault_paths(
      " Folder\\Note.md \n"
      "Folder/Note.md\n"
      "Folder/Note.exe\n"
      "Image.PNG\n"
      "Molecule.PDB\n"
      "\n",
      &paths));

  require_true(paths.count == 3, "supported path filter should keep only app-supported paths");
  require_true(
      std::string(paths.paths[0]) == "Folder/Note.md",
      "supported path filter should trim and normalize backslashes");
  require_true(
      std::string(paths.paths[1]) == "Image.PNG",
      "supported path filter should preserve original path case");
  require_true(
      std::string(paths.paths[2]) == "Molecule.PDB",
      "supported path filter should keep chemistry structure paths");
  kernel_free_path_list(&paths);
  require_true(paths.paths == nullptr && paths.count == 0, "supported path free should reset output");
  require_true(
      kernel_filter_supported_vault_paths("note.md", nullptr).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "supported path filter should require output pointer");
}

void test_filter_supported_vault_paths_filtered_applies_hidden_and_ignored_roots() {
  kernel_path_list paths{};
  expect_ok(kernel_filter_supported_vault_paths_filtered(
      " Folder\\Note.md \n"
      "Folder/Note.md\n"
      ".hidden/Note.md\n"
      "Folder/.hidden/Note.md\n"
      "node_modules/Note.md\n"
      "Other.PDF\n"
      "Other.exe\n",
      " node_modules ",
      &paths));

  require_true(paths.count == 2, "filtered supported path filter should drop hidden and ignored roots");
  require_true(
      std::string(paths.paths[0]) == "Folder/Note.md",
      "filtered supported path filter should still trim, normalize, and deduplicate");
  require_true(
      std::string(paths.paths[1]) == "Other.PDF",
      "filtered supported path filter should keep supported non-Markdown files");
  kernel_free_path_list(&paths);
  require_true(
      kernel_filter_supported_vault_paths_filtered("note.md", "node_modules", nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "filtered supported path filter should require output pointer");
}

}  // namespace

void run_kernel_api_core_vault_entry_contract_tests() {
  test_create_folder_creates_single_directory();
  test_rename_note_updates_catalog_and_preserves_content();
  test_delete_note_removes_catalog_entry();
  test_move_note_updates_catalog();
  test_filter_changed_markdown_paths_normalizes_filters_and_deduplicates();
  test_filter_supported_vault_paths_normalizes_filters_and_deduplicates();
  test_filter_supported_vault_paths_filtered_applies_hidden_and_ignored_roots();
}
