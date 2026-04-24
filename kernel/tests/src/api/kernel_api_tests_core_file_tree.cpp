// Reason: This file verifies the kernel-owned file tree surface consumed by
// the Tauri shell.

#include "kernel/c_api.h"

#include "api/kernel_api_core_base_suites.h"
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

void test_query_file_tree_builds_sorted_folder_first_tree() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  write_note(handle, "zeta.md", "# Zeta\n");
  write_note(handle, "lab/notes/b.md", "# B\n");
  write_note(handle, "lab/a.md", "# A\n");
  write_note(handle, "alpha.md", "# Alpha\n");

  kernel_file_tree tree{};
  expect_ok(kernel_query_file_tree(handle, 16, &tree));
  require_true(tree.count == 3, "file tree should have one folder plus two root notes");
  require_true(tree.nodes[0].is_folder != 0, "folders should sort before files");
  require_true(std::string(tree.nodes[0].name) == "lab", "folder node should preserve name");
  require_true(std::string(tree.nodes[0].relative_path) == "lab", "folder node should expose rel path");
  require_true(tree.nodes[0].file_count == 2, "folder count should include nested notes");
  require_true(tree.nodes[0].child_count == 2, "lab folder should have note and subfolder children");
  require_true(
      std::string(tree.nodes[0].children[0].name) == "notes",
      "nested folder should sort before file leaf");
  require_true(
      std::string(tree.nodes[0].children[1].relative_path) == "lab/a.md",
      "file leaf should preserve rel path");
  require_true(tree.nodes[0].children[1].has_note != 0, "file leaf should carry note payload");
  require_true(
      std::string(tree.nodes[0].children[1].note.name) == "a",
      "note payload should expose file stem");
  require_true(
      std::string(tree.nodes[0].children[1].note.extension) == "md",
      "note payload should expose extension");
  require_true(tree.nodes[1].is_folder == 0, "root notes should follow folders");
  require_true(std::string(tree.nodes[1].relative_path) == "alpha.md", "alpha should sort first");
  require_true(std::string(tree.nodes[2].relative_path) == "zeta.md", "zeta should sort last");

  kernel_free_file_tree(&tree);
  require_true(tree.nodes == nullptr && tree.count == 0, "file tree free should reset output");
  kernel_free_file_tree(&tree);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_query_file_tree_limit_is_applied_before_tree_construction() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  write_note(handle, "b.md", "# B\n");
  write_note(handle, "a.md", "# A\n");

  kernel_file_tree tree{};
  expect_ok(kernel_query_file_tree(handle, 1, &tree));
  require_true(tree.count == 1, "file tree limit should cap source notes");
  require_true(std::string(tree.nodes[0].relative_path) == "a.md", "limited tree should still sort");
  kernel_free_file_tree(&tree);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_query_file_tree_requires_valid_arguments() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_file_tree tree{};
  require_true(
      kernel_query_file_tree(nullptr, 1, &tree).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "file tree should require handle");
  require_true(
      kernel_query_file_tree(handle, 0, &tree).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "file tree should reject zero limit");
  require_true(
      kernel_query_file_tree(handle, 1, nullptr).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "file tree should require output pointer");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_kernel_api_core_file_tree_contract_tests() {
  test_query_file_tree_builds_sorted_folder_first_tree();
  test_query_file_tree_limit_is_applied_before_tree_construction();
  test_query_file_tree_requires_valid_arguments();
}
