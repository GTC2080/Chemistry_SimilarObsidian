// Reason: This file isolates legacy tag and backlinks coverage so note-search regression files stay smaller and easier to scan.

#include "kernel/c_api.h"

#include "api/kernel_api_search_legacy_suites.h"
#include "api/kernel_api_test_support.h"
#include "core/kernel_internal.h"
#include "recovery/journal.h"
#include "support/test_support.h"

#include <filesystem>
#include <mutex>
#include <string>

namespace {

void test_tag_query_returns_matching_notes_in_rel_path_order() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string a_text = "# A\n#sharedtag\n";
  const std::string b_text = "# B\n#sharedtag\n";
  const std::string c_text = "# C\n#othertag\n";

  expect_ok(kernel_write_note(handle, "b-note.md", a_text.data(), a_text.size(), nullptr, &metadata, &disposition));
  expect_ok(kernel_write_note(handle, "a-note.md", b_text.data(), b_text.size(), nullptr, &metadata, &disposition));
  expect_ok(kernel_write_note(handle, "c-note.md", c_text.data(), c_text.size(), nullptr, &metadata, &disposition));

  kernel_search_results results{};
  expect_ok(kernel_query_tag_notes(handle, "sharedtag", static_cast<size_t>(-1), &results));
  require_true(results.count == 2, "tag query should return two matching notes");
  require_true(std::string(results.hits[0].rel_path) == "a-note.md", "tag query should order hits by rel_path ascending");
  require_true(std::string(results.hits[0].title) == "B", "tag query should preserve note titles");
  require_true(results.hits[0].match_flags == KERNEL_SEARCH_MATCH_NONE, "tag query hits should report no TITLE/BODY flags");
  require_true(std::string(results.hits[1].rel_path) == "b-note.md", "tag query should order hits by rel_path ascending");
  require_true(std::string(results.hits[1].title) == "A", "tag query should preserve note titles");
  require_true(results.hits[1].match_flags == KERNEL_SEARCH_MATCH_NONE, "tag query hits should report no TITLE/BODY flags");
  kernel_free_search_results(&results);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_backlinks_query_returns_matching_sources() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string target_text = "# Target Note\nbody\n";
  const std::string source_a_text = "# Source A\n[[Target Note]]\n";
  const std::string source_b_text = "# Source B\n[[Target Note]]\n";
  const std::string unrelated_text = "# Unrelated\n[[Other Note]]\n";

  expect_ok(kernel_write_note(handle, "target.md", target_text.data(), target_text.size(), nullptr, &metadata, &disposition));
  expect_ok(kernel_write_note(handle, "b-source.md", source_a_text.data(), source_a_text.size(), nullptr, &metadata, &disposition));
  expect_ok(kernel_write_note(handle, "a-source.md", source_b_text.data(), source_b_text.size(), nullptr, &metadata, &disposition));
  expect_ok(kernel_write_note(handle, "c-unrelated.md", unrelated_text.data(), unrelated_text.size(), nullptr, &metadata, &disposition));

  kernel_search_results results{};
  expect_ok(kernel_query_backlinks(handle, "target.md", static_cast<size_t>(-1), &results));
  require_true(results.count == 2, "backlinks query should return two matching source notes");
  require_true(std::string(results.hits[0].rel_path) == "a-source.md", "backlinks query should order hits by rel_path ascending");
  require_true(std::string(results.hits[0].title) == "Source B", "backlinks query should preserve source titles");
  require_true(results.hits[0].match_flags == KERNEL_SEARCH_MATCH_NONE, "backlinks query hits should report no TITLE/BODY flags");
  require_true(std::string(results.hits[1].rel_path) == "b-source.md", "backlinks query should order hits by rel_path ascending");
  require_true(std::string(results.hits[1].title) == "Source A", "backlinks query should preserve source titles");
  require_true(results.hits[1].match_flags == KERNEL_SEARCH_MATCH_NONE, "backlinks query hits should report no TITLE/BODY flags");
  kernel_free_search_results(&results);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_backlinks_query_accepts_windows_style_relative_path() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string target_text = "# Nested Target\nbody\n";
  const std::string source_text = "# Nested Source\n[[Nested Target]]\n";

  expect_ok(kernel_write_note(handle, "nested/target.md", target_text.data(), target_text.size(), nullptr, &metadata, &disposition));
  expect_ok(kernel_write_note(handle, "nested/source.md", source_text.data(), source_text.size(), nullptr, &metadata, &disposition));

  kernel_search_results results{};
  expect_ok(kernel_query_backlinks(handle, "nested/target.md", static_cast<size_t>(-1), &results));
  require_true(results.count == 1, "forward-slash backlinks query should find the nested source");
  require_true(std::string(results.hits[0].rel_path) == "nested/source.md", "forward-slash backlinks query should preserve source rel_path");

  expect_ok(kernel_query_backlinks(handle, "nested\\target.md", static_cast<size_t>(-1), &results));
  require_true(results.count == 1, "backlinks query should normalize Windows-style separators");
  require_true(std::string(results.hits[0].rel_path) == "nested/source.md", "normalized backlinks query should preserve source rel_path");
  require_true(std::string(results.hits[0].title) == "Nested Source", "normalized backlinks query should preserve source title");
  kernel_free_search_results(&results);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_tag_and_backlinks_queries_follow_rewrite_recovery_and_rebuild() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string target_text = "# Stable Target\nbody\n";
  const std::string original_source_text = "# Source\n#oldtag\n[[Stable Target]]\n";
  expect_ok(kernel_write_note(handle, "target.md", target_text.data(), target_text.size(), nullptr, &metadata, &disposition));
  expect_ok(kernel_write_note(handle, "source.md", original_source_text.data(), original_source_text.size(), nullptr, &metadata, &disposition));

  const std::string rewritten_source_text = "# Source\n#newtag\n[[Other Target]]\n";
  expect_ok(kernel_write_note(
      handle,
      "source.md",
      rewritten_source_text.data(),
      rewritten_source_text.size(),
      metadata.content_revision,
      &metadata,
      &disposition));

  kernel_search_results results{};
  expect_ok(kernel_query_tag_notes(handle, "oldtag", static_cast<size_t>(-1), &results));
  require_true(results.count == 0, "rewrite should remove old tag query hits");
  kernel_free_search_results(&results);
  expect_ok(kernel_query_backlinks(handle, "target.md", static_cast<size_t>(-1), &results));
  require_true(results.count == 0, "rewrite should remove stale backlinks query hits");
  kernel_free_search_results(&results);

  {
    std::lock_guard lock(handle->storage_mutex);
    exec_sql(handle->storage.connection, "DELETE FROM note_tags WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='source.md');");
    exec_sql(handle->storage.connection, "INSERT INTO note_tags(note_id, tag) VALUES((SELECT note_id FROM notes WHERE rel_path='source.md'), 'staletag');");
    exec_sql(handle->storage.connection, "DELETE FROM note_links WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='source.md');");
    exec_sql(handle->storage.connection, "INSERT INTO note_links(note_id, target) VALUES((SELECT note_id FROM notes WHERE rel_path='source.md'), 'Stable Target');");
  }

  expect_ok(kernel_rebuild_index(handle));

  expect_ok(kernel_query_tag_notes(handle, "staletag", static_cast<size_t>(-1), &results));
  require_true(results.count == 0, "rebuild should remove stale tag query hits");
  kernel_free_search_results(&results);
  expect_ok(kernel_query_tag_notes(handle, "newtag", static_cast<size_t>(-1), &results));
  require_true(results.count == 1, "rebuild should restore live tag query hits");
  kernel_free_search_results(&results);
  expect_ok(kernel_query_backlinks(handle, "target.md", static_cast<size_t>(-1), &results));
  require_true(results.count == 0, "rebuild should remove stale backlink query hits");
  kernel_free_search_results(&results);

  expect_ok(kernel_close(handle));

  prepare_state_dir_for_vault(vault);
  const auto journal_path = journal_path_for_vault(vault);
  const auto temp_path = vault / "source.md.recovery.tmp";
  const std::string recovered_source_text = "# Source\n#recovertag\n[[Stable Target]]\n";
  write_file_bytes(vault / "source.md", recovered_source_text);
  write_file_bytes(temp_path, "stale temp");
  require_true(
      !kernel::recovery::append_save_begin(
          journal_path,
          "tag-backlink-recovery-op",
          "source.md",
          temp_path),
      "recovery journal append should succeed");

  handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "recovery-backed tag/backlink query test should settle to READY");

  expect_ok(kernel_query_tag_notes(handle, "recovertag", static_cast<size_t>(-1), &results));
  require_true(results.count == 1, "startup recovery should restore recovered tag query hits");
  kernel_free_search_results(&results);
  expect_ok(kernel_query_backlinks(handle, "target.md", static_cast<size_t>(-1), &results));
  require_true(results.count == 1, "startup recovery should restore recovered backlink query hits");
  require_true(std::string(results.hits[0].rel_path) == "source.md", "recovered backlink hit should preserve rel_path");
  kernel_free_search_results(&results);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_tag_query_rejects_invalid_inputs_and_supports_limit() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(handle, "b-note.md", "# B\n#limitag\n", 12, nullptr, &metadata, &disposition));
  expect_ok(kernel_write_note(handle, "a-note.md", "# A\n#limitag\n", 12, nullptr, &metadata, &disposition));

  kernel_search_results results{};
  require_true(
      kernel_query_tag_notes(handle, "", static_cast<size_t>(-1), &results).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "empty tag query should be invalid");
  require_true(
      kernel_query_tag_notes(handle, "   \t", static_cast<size_t>(-1), &results).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "whitespace-only tag query should be invalid");
  require_true(
      kernel_query_tag_notes(handle, "limitag", 0, &results).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "zero-limit tag query should be invalid");

  expect_ok(kernel_query_tag_notes(handle, "limitag", 1, &results));
  require_true(results.count == 1, "tag query limit should cap results");
  require_true(std::string(results.hits[0].rel_path) == "a-note.md", "tag query limit should keep rel_path ordering");
  require_true(std::string(results.hits[0].title) == "A", "tag query limit should preserve hit titles");
  require_true(results.hits[0].match_flags == KERNEL_SEARCH_MATCH_NONE, "tag query hits should report no TITLE/BODY flags");

  const kernel_status invalid_status = kernel_query_tag_notes(handle, "", static_cast<size_t>(-1), &results);
  const bool invalid_cleared = results.count == 0 && results.hits == nullptr;
  if (!invalid_cleared) {
    kernel_free_search_results(&results);
  }
  require_true(invalid_status.code == KERNEL_ERROR_INVALID_ARGUMENT, "invalid tag query should still report INVALID_ARGUMENT");
  require_true(invalid_cleared, "invalid tag query should clear stale output hits");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_backlinks_query_rejects_invalid_inputs_and_supports_limit() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string target_text = "# Target Note\nbody\n";
  const std::string source_a_text = "# Source A\n[[Target Note]]\n";
  const std::string source_b_text = "# Source B\n[[Target Note]]\n";

  expect_ok(kernel_write_note(handle, "target.md", target_text.data(), target_text.size(), nullptr, &metadata, &disposition));
  expect_ok(kernel_write_note(handle, "b-source.md", source_a_text.data(), source_a_text.size(), nullptr, &metadata, &disposition));
  expect_ok(kernel_write_note(handle, "a-source.md", source_b_text.data(), source_b_text.size(), nullptr, &metadata, &disposition));

  kernel_search_results results{};
  require_true(
      kernel_query_backlinks(handle, "", static_cast<size_t>(-1), &results).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "empty backlinks rel_path should be invalid");
  require_true(
      kernel_query_backlinks(handle, "..\\target.md", static_cast<size_t>(-1), &results).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "path-traversal backlinks rel_path should be invalid");
  require_true(
      kernel_query_backlinks(handle, "target.md", 0, &results).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "zero-limit backlinks query should be invalid");

  expect_ok(kernel_query_backlinks(handle, "target.md", 1, &results));
  require_true(results.count == 1, "backlinks query limit should cap results");
  require_true(std::string(results.hits[0].rel_path) == "a-source.md", "backlinks query limit should keep rel_path ordering");
  require_true(std::string(results.hits[0].title) == "Source B", "backlinks query limit should preserve source titles");
  require_true(results.hits[0].match_flags == KERNEL_SEARCH_MATCH_NONE, "backlinks query hits should report no TITLE/BODY flags");

  const kernel_status invalid_status = kernel_query_backlinks(handle, "", static_cast<size_t>(-1), &results);
  const bool invalid_cleared = results.count == 0 && results.hits == nullptr;
  if (!invalid_cleared) {
    kernel_free_search_results(&results);
  }
  require_true(invalid_status.code == KERNEL_ERROR_INVALID_ARGUMENT, "invalid backlinks query should still report INVALID_ARGUMENT");
  require_true(invalid_cleared, "invalid backlinks query should clear stale output hits");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_search_legacy_graph_tests() {
  test_tag_query_returns_matching_notes_in_rel_path_order();
  test_backlinks_query_returns_matching_sources();
  test_backlinks_query_accepts_windows_style_relative_path();
  test_tag_and_backlinks_queries_follow_rewrite_recovery_and_rebuild();
  test_tag_query_rejects_invalid_inputs_and_supports_limit();
  test_backlinks_query_rejects_invalid_inputs_and_supports_limit();
}
