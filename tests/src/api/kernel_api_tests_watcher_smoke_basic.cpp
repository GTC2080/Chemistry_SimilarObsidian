// Reason: This file isolates steady-state watcher smoke coverage so close and catch-up edges can live separately.

#include "kernel/c_api.h"

#include "api/kernel_api_test_support.h"
#include "api/kernel_api_watcher_smoke_suites.h"
#include "support/test_support.h"

#include <filesystem>
#include <string>

namespace {

void test_background_watcher_indexes_external_create() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  write_file_bytes(
      vault / "bg-create.md",
      "# Background Create\nbg-create-token\n");

  require_eventually(
      [&]() {
        kernel_search_results results{};
        const kernel_status status = kernel_search_notes(handle, "bg-create-token", &results);
        if (status.code != KERNEL_OK) {
          return false;
        }
        const bool matched =
            results.count == 1 &&
            std::string(results.hits[0].rel_path) == "bg-create.md";
        kernel_free_search_results(&results);
        return matched;
      },
      "background watcher should index external create");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_background_watcher_updates_external_modify() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string original =
      "# Background Modify\n"
      "bg-before-token\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "bg-modify.md",
      original.data(),
      original.size(),
      nullptr,
      &metadata,
      &disposition));

  write_file_bytes(
      vault / "bg-modify.md",
      "# Background Modify\n"
      "bg-after-token\n");

  require_eventually(
      [&]() {
        kernel_search_results old_results{};
        if (kernel_search_notes(handle, "bg-before-token", &old_results).code != KERNEL_OK) {
          return false;
        }
        const bool old_gone = old_results.count == 0;
        kernel_free_search_results(&old_results);

        kernel_search_results new_results{};
        if (kernel_search_notes(handle, "bg-after-token", &new_results).code != KERNEL_OK) {
          return false;
        }
        const bool new_present =
            new_results.count == 1 &&
            std::string(new_results.hits[0].rel_path) == "bg-modify.md";
        kernel_free_search_results(&new_results);
        return old_gone && new_present;
      },
      "background watcher should replace stale search rows after external modify");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_background_watcher_applies_external_delete() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Background Delete\n"
      "bg-delete-token\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "bg-delete.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));

  std::filesystem::remove(vault / "bg-delete.md");

  require_eventually(
      [&]() {
        kernel_search_results results{};
        if (kernel_search_notes(handle, "bg-delete-token", &results).code != KERNEL_OK) {
          return false;
        }
        const bool removed = results.count == 0;
        kernel_free_search_results(&results);
        if (!removed) {
          return false;
        }

        kernel_state_snapshot snapshot{};
        if (kernel_get_state(handle, &snapshot).code != KERNEL_OK) {
          return false;
        }
        return snapshot.indexed_note_count == 0;
      },
      "background watcher should remove deleted note from search and active count");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_internal_write_suppression_does_not_swallow_later_external_modify() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "suppression test should start from a ready state");

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string initial_text = "# Initial Title\ninitial-token\n";
  expect_ok(
      kernel_write_note(
          handle,
          "suppressed.md",
          initial_text.data(),
          initial_text.size(),
          nullptr,
          &metadata,
          &disposition));
  require_true(disposition == KERNEL_WRITE_WRITTEN, "initial internal write should persist note");

  const std::string external_text = "# External Title\nexternal-after-internal-token\n";
  write_file_bytes(vault / "suppressed.md", external_text);

  require_eventually(
      [&]() {
        kernel_search_results results{};
        const kernel_status status =
            kernel_search_notes(handle, "external-after-internal-token", &results);
        if (status.code != KERNEL_OK) {
          return false;
        }
        const bool matched =
            results.count == 1 &&
            std::string(results.hits[0].rel_path) == "suppressed.md";
        kernel_free_search_results(&results);
        return matched;
      },
      "external modify after internal write should not be swallowed by watcher suppression");

  kernel_search_results results{};
  expect_ok(kernel_search_notes(handle, "initial-token", &results));
  require_true(results.count == 0, "later external modify should replace the stale self-written FTS row");
  kernel_free_search_results(&results);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_kernel_api_watcher_basic_smoke_tests() {
  test_background_watcher_indexes_external_create();
  test_background_watcher_updates_external_modify();
  test_background_watcher_applies_external_delete();
  test_internal_write_suppression_does_not_swallow_later_external_modify();
}
