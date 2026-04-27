// Reason: This file verifies the host-facing tag summary and tag-note query surfaces.

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
  const kernel_status status = kernel_write_note(
      handle,
      rel_path,
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition);
  require_true(status.code == KERNEL_OK, "tag catalog setup note write should succeed");
  require_true(disposition == KERNEL_WRITE_WRITTEN, "test note should be written");
}

void test_query_tags_returns_live_summaries_sorted_by_count_then_name() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  write_note(handle, "alpha.md", "# Alpha\nTags: #chem #chem/nmr\n");
  write_note(handle, "beta.md", "# Beta\nTags: #chem\n");
  write_note(handle, "gamma.md", "# Gamma\nTags: #bio\n");

  kernel_tag_list tags{};
  expect_ok(kernel_query_tags(handle, 16, &tags));
  require_true(tags.count == 3, "tag summary should include live tags only");
  require_true(std::string(tags.tags[0].name) == "chem", "tag summary should sort highest count first");
  require_true(tags.tags[0].count == 2, "tag summary should expose note count");
  require_true(std::string(tags.tags[1].name) == "bio", "tag summary should sort equal counts by name");
  require_true(tags.tags[1].count == 1, "tag summary should expose single-note count");
  require_true(std::string(tags.tags[2].name) == "chem/nmr", "tag summary should preserve nested tag path");
  kernel_free_tag_list(&tags);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_query_tags_limit_and_argument_validation() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  write_note(handle, "alpha.md", "# Alpha\nTags: #chem #org\n");

  kernel_tag_list tags{};
  expect_ok(kernel_query_tags(handle, 1, &tags));
  require_true(tags.count == 1, "tag summary limit should cap results");
  kernel_free_tag_list(&tags);

  require_true(
      kernel_query_tags(nullptr, 1, &tags).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "tag summary should require handle");
  require_true(
      kernel_query_tags(handle, 0, &tags).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "tag summary should reject zero limit");
  require_true(
      kernel_query_tags(handle, 1, nullptr).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "tag summary should require output pointer");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_tag_default_limits_are_kernel_owned() {
  size_t tag_catalog_limit = 0;
  expect_ok(kernel_get_tag_catalog_default_limit(&tag_catalog_limit));
  require_true(tag_catalog_limit == 512, "tag catalog default limit should be kernel-owned");
  require_true(
      kernel_get_tag_catalog_default_limit(nullptr).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "tag catalog default limit should require output pointer");

  size_t tag_note_limit = 0;
  expect_ok(kernel_get_tag_note_default_limit(&tag_note_limit));
  require_true(tag_note_limit == 128, "tag-note default limit should be kernel-owned");
  require_true(
      kernel_get_tag_note_default_limit(nullptr).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "tag-note default limit should require output pointer");

  size_t tag_tree_limit = 0;
  expect_ok(kernel_get_tag_tree_default_limit(&tag_tree_limit));
  require_true(tag_tree_limit == 512, "tag-tree default limit should be kernel-owned");
  require_true(
      kernel_get_tag_tree_default_limit(nullptr).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "tag-tree default limit should require output pointer");
}

void test_query_tag_notes_uses_live_kernel_tag_index() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  write_note(handle, "b.md", "# B\nTags: #chem\n");
  write_note(handle, "a.md", "# A\nTags: #chem #org\n");

  kernel_search_results results{};
  expect_ok(kernel_query_tag_notes(handle, "chem", 16, &results));
  require_true(results.count == 2, "tag notes should include matching live notes");
  require_true(std::string(results.hits[0].rel_path) == "a.md", "tag notes should sort by rel_path");
  require_true(std::string(results.hits[1].rel_path) == "b.md", "tag notes should include second match");
  kernel_free_search_results(&results);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_kernel_api_core_tag_contract_tests() {
  test_query_tags_returns_live_summaries_sorted_by_count_then_name();
  test_query_tags_limit_and_argument_validation();
  test_tag_default_limits_are_kernel_owned();
  test_query_tag_notes_uses_live_kernel_tag_index();
}
