// Reason: This file pins the kernel-owned AI embedding cache surface before the host DB is retired.

#include "kernel/c_api.h"

#include "support/test_support.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace {

void require_timestamp_record(
    const kernel_ai_embedding_timestamp_list& timestamps,
    const std::size_t index,
    const char* rel_path,
    const int64_t updated_at) {
  require_true(index < timestamps.count, "timestamp record index should be in range");
  require_true(
      std::string(timestamps.records[index].rel_path) == rel_path,
      "timestamp record should preserve rel_path");
  require_true(
      timestamps.records[index].updated_at == updated_at,
      "timestamp record should preserve updated_at");
}

void write_note_for_embedding_test(
    kernel_handle* handle,
    const char* rel_path,
    std::string_view content) {
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
}

int64_t note_updated_at(kernel_handle* handle, const char* rel_path) {
  kernel_note_list notes{};
  expect_ok(kernel_query_notes(handle, 100, &notes));
  for (std::size_t index = 0; index < notes.count; ++index) {
    if (std::string(notes.notes[index].rel_path) == rel_path) {
      const int64_t updated_at = static_cast<int64_t>(notes.notes[index].mtime_ns / 1000000000ULL);
      kernel_free_note_list(&notes);
      return updated_at;
    }
  }
  kernel_free_note_list(&notes);
  require_true(false, "note should be present in catalog");
  return 0;
}

void require_refresh_job(
    const kernel_ai_embedding_refresh_job_list& jobs,
    const std::size_t index,
    const char* rel_path,
    const char* title,
    std::string_view content) {
  require_true(index < jobs.count, "refresh job index should be in range");
  require_true(
      std::string(jobs.jobs[index].rel_path) == rel_path,
      "refresh job should preserve rel_path");
  require_true(
      std::string(jobs.jobs[index].title) == title,
      "refresh job should preserve title");
  require_true(
      std::string(jobs.jobs[index].content, jobs.jobs[index].content_size) == content,
      "refresh job should carry note content");
  require_true(jobs.jobs[index].created_at > 0, "refresh job should expose created_at");
  require_true(jobs.jobs[index].updated_at > 0, "refresh job should expose updated_at");
}

void test_embedding_cache_metadata_vectors_and_top_notes_are_kernel_owned() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const kernel_ai_embedding_note_metadata alpha{
      "alpha.md",
      "Alpha",
      "C:/vault/alpha.md",
      100,
      200};
  const kernel_ai_embedding_note_metadata beta{
      "nested/beta.md",
      "Beta",
      "C:/vault/nested/beta.md",
      110,
      250};

  expect_ok(kernel_upsert_ai_embedding_note_metadata(handle, &beta));
  expect_ok(kernel_upsert_ai_embedding_note_metadata(handle, &alpha));

  kernel_ai_embedding_timestamp_list timestamps{};
  expect_ok(kernel_query_ai_embedding_note_timestamps(handle, &timestamps));
  require_true(timestamps.count == 2, "embedding timestamp cache should expose two records");
  require_timestamp_record(timestamps, 0, "alpha.md", 200);
  require_timestamp_record(timestamps, 1, "nested/beta.md", 250);
  kernel_free_ai_embedding_timestamp_list(&timestamps);

  const float alpha_vector[] = {1.0f, 0.0f};
  const float beta_vector[] = {0.0f, 1.0f};
  expect_ok(kernel_update_ai_embedding(handle, "alpha.md", alpha_vector, 2));
  expect_ok(kernel_update_ai_embedding(handle, "nested/beta.md", beta_vector, 2));

  const float query[] = {0.9f, 0.1f};
  kernel_search_results top_notes{};
  expect_ok(kernel_query_ai_embedding_top_notes(handle, query, 2, nullptr, 2, &top_notes));
  require_true(top_notes.count == 2, "embedding top-k should return both cached notes");
  require_true(
      std::string(top_notes.hits[0].rel_path) == "alpha.md",
      "embedding top-k should rank the closest note first");
  require_true(
      std::string(top_notes.hits[0].title) == "Alpha",
      "embedding top-k should preserve note title");
  require_true(
      std::string(top_notes.hits[1].rel_path) == "nested/beta.md",
      "embedding top-k should include the second-best note");
  kernel_free_search_results(&top_notes);

  expect_ok(kernel_query_ai_embedding_top_notes(handle, query, 2, "alpha.md", 2, &top_notes));
  require_true(top_notes.count == 1, "embedding top-k should honor the excluded active note");
  require_true(
      std::string(top_notes.hits[0].rel_path) == "nested/beta.md",
      "embedding top-k should return the next note after exclusion");
  kernel_free_search_results(&top_notes);

  const kernel_ai_embedding_note_metadata alpha_refreshed{
      "alpha.md",
      "Alpha refreshed",
      "C:/vault/alpha.md",
      100,
      300};
  expect_ok(kernel_upsert_ai_embedding_note_metadata(handle, &alpha_refreshed));
  expect_ok(kernel_query_ai_embedding_top_notes(handle, query, 2, nullptr, 2, &top_notes));
  require_true(
      top_notes.count == 1,
      "refreshing embedding metadata should clear the stale vector for that note");
  require_true(
      std::string(top_notes.hits[0].rel_path) == "nested/beta.md",
      "metadata refresh should leave other note vectors queryable");
  kernel_free_search_results(&top_notes);

  expect_ok(kernel_clear_ai_embeddings(handle));
  expect_ok(kernel_query_ai_embedding_top_notes(handle, query, 2, nullptr, 2, &top_notes));
  require_true(top_notes.count == 0, "clearing embeddings should keep metadata but remove vector hits");
  kernel_free_search_results(&top_notes);

  uint8_t deleted = 0;
  expect_ok(kernel_delete_ai_embedding_note(handle, "nested/beta.md", &deleted));
  require_true(deleted == 1, "embedding delete should report a removed metadata row");
  expect_ok(kernel_delete_ai_embedding_note(handle, "missing.md", &deleted));
  require_true(deleted == 0, "embedding delete should report a no-op for missing notes");

  expect_ok(kernel_query_ai_embedding_note_timestamps(handle, &timestamps));
  require_true(timestamps.count == 1, "embedding metadata delete should remove the timestamp row");
  require_timestamp_record(timestamps, 0, "alpha.md", 300);
  kernel_free_ai_embedding_timestamp_list(&timestamps);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_embedding_refresh_jobs_are_prepared_in_kernel() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "ignored");
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  write_note_for_embedding_test(handle, "fresh.md", "# Fresh\nUseful chemistry notes.\n");
  write_note_for_embedding_test(handle, "old.md", "# Old\nNeeds a newer vector.\n");
  write_note_for_embedding_test(handle, "cached.md", "# Cached\nAlready embedded.\n");
  write_note_for_embedding_test(handle, "blank.md", "   \n\t  ");
  write_note_for_embedding_test(handle, "ignored/skip.md", "# Skip\nIgnored root.\n");

  const int64_t old_updated_at = note_updated_at(handle, "old.md");
  const int64_t cached_updated_at = note_updated_at(handle, "cached.md");
  const std::string old_absolute_path = (vault / "old.md").string();
  const std::string cached_absolute_path = (vault / "cached.md").string();

  const kernel_ai_embedding_note_metadata old_metadata{
      "old.md",
      "Old",
      old_absolute_path.c_str(),
      old_updated_at,
      old_updated_at - 1};
  const kernel_ai_embedding_note_metadata cached_metadata{
      "cached.md",
      "Cached",
      cached_absolute_path.c_str(),
      cached_updated_at,
      cached_updated_at};
  expect_ok(kernel_upsert_ai_embedding_note_metadata(handle, &old_metadata));
  expect_ok(kernel_upsert_ai_embedding_note_metadata(handle, &cached_metadata));

  kernel_ai_embedding_refresh_job_list jobs{};
  expect_ok(kernel_prepare_ai_embedding_refresh_jobs(handle, "ignored", 100, 0, &jobs));
  require_true(jobs.count == 2, "kernel should return only uncached or stale indexable jobs");
  require_refresh_job(jobs, 0, "fresh.md", "Fresh", "# Fresh\nUseful chemistry notes.\n");
  require_refresh_job(jobs, 1, "old.md", "Old", "# Old\nNeeds a newer vector.\n");
  kernel_free_ai_embedding_refresh_job_list(&jobs);

  kernel_ai_embedding_timestamp_list timestamps{};
  expect_ok(kernel_query_ai_embedding_note_timestamps(handle, &timestamps));
  require_true(
      timestamps.count == 3,
      "preparing jobs should upsert metadata for returned embedding jobs");
  kernel_free_ai_embedding_timestamp_list(&timestamps);

  expect_ok(kernel_prepare_ai_embedding_refresh_jobs(handle, "ignored", 100, 1, &jobs));
  require_true(jobs.count == 3, "force refresh should include all indexable visible notes");
  require_refresh_job(jobs, 0, "cached.md", "Cached", "# Cached\nAlready embedded.\n");
  require_refresh_job(jobs, 1, "fresh.md", "Fresh", "# Fresh\nUseful chemistry notes.\n");
  require_refresh_job(jobs, 2, "old.md", "Old", "# Old\nNeeds a newer vector.\n");
  kernel_free_ai_embedding_refresh_job_list(&jobs);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_changed_embedding_refresh_jobs_refresh_catalog_in_kernel() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  write_file_bytes(vault / "external.md", "# External\nCreated outside kernel write.\n");
  write_file_bytes(vault / "external.txt", "not markdown");

  kernel_ai_embedding_refresh_job_list jobs{};
  expect_ok(kernel_prepare_changed_ai_embedding_refresh_jobs(
      handle,
      " external.md \nexternal.txt\nexternal.md",
      100,
      &jobs));
  require_true(jobs.count == 1, "changed job planner should normalize and dedupe markdown paths");
  require_refresh_job(
      jobs,
      0,
      "external.md",
      "External",
      "# External\nCreated outside kernel write.\n");
  kernel_free_ai_embedding_refresh_job_list(&jobs);

  kernel_note_list notes{};
  expect_ok(kernel_query_notes(handle, 100, &notes));
  require_true(notes.count == 1, "changed job planner should refresh the kernel note catalog");
  require_true(
      std::string(notes.notes[0].rel_path) == "external.md",
      "changed job planner should index the external markdown path");
  kernel_free_note_list(&notes);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_embedding_cache_rejects_invalid_arguments() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const kernel_ai_embedding_note_metadata valid{
      "alpha.md",
      "Alpha",
      "C:/vault/alpha.md",
      100,
      200};
  const kernel_ai_embedding_note_metadata invalid_rel_path{
      "",
      "Alpha",
      "C:/vault/alpha.md",
      100,
      200};
  const float values[] = {1.0f};

  require_true(
      kernel_upsert_ai_embedding_note_metadata(nullptr, &valid).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "embedding metadata upsert should require a handle");
  require_true(
      kernel_upsert_ai_embedding_note_metadata(handle, nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "embedding metadata upsert should require metadata");
  require_true(
      kernel_upsert_ai_embedding_note_metadata(handle, &invalid_rel_path).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "embedding metadata upsert should reject an empty rel_path");

  kernel_ai_embedding_timestamp_list timestamps{};
  require_true(
      kernel_query_ai_embedding_note_timestamps(nullptr, &timestamps).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "embedding timestamp query should require a handle");
  require_true(
      kernel_query_ai_embedding_note_timestamps(handle, nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "embedding timestamp query should require an output pointer");

  kernel_ai_embedding_refresh_job_list jobs{};
  require_true(
      kernel_prepare_ai_embedding_refresh_jobs(nullptr, "", 1, 0, &jobs).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "embedding refresh jobs should require a handle");
  require_true(
      kernel_prepare_ai_embedding_refresh_jobs(handle, "", 0, 0, &jobs).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "embedding refresh jobs should reject zero limit");
  require_true(
      kernel_prepare_ai_embedding_refresh_jobs(handle, "", 1, 0, nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "embedding refresh jobs should require an output list");
  require_true(
      kernel_prepare_changed_ai_embedding_refresh_jobs(nullptr, "alpha.md", 1, &jobs).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "changed embedding refresh jobs should require a handle");
  require_true(
      kernel_prepare_changed_ai_embedding_refresh_jobs(handle, "alpha.md", 0, &jobs).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "changed embedding refresh jobs should reject zero limit");
  require_true(
      kernel_prepare_changed_ai_embedding_refresh_jobs(handle, "alpha.md", 1, nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "changed embedding refresh jobs should require an output list");

  require_true(
      kernel_update_ai_embedding(nullptr, "alpha.md", values, 1).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "embedding update should require a handle");
  require_true(
      kernel_update_ai_embedding(handle, "", values, 1).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "embedding update should require a rel_path");
  require_true(
      kernel_update_ai_embedding(handle, "alpha.md", nullptr, 1).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "embedding update should require vector values");
  require_true(
      kernel_update_ai_embedding(handle, "alpha.md", values, 0).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "embedding update should reject empty vectors");

  kernel_search_results top_notes{};
  require_true(
      kernel_query_ai_embedding_top_notes(nullptr, values, 1, nullptr, 1, &top_notes).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "embedding top-k should require a handle");
  require_true(
      kernel_query_ai_embedding_top_notes(handle, nullptr, 1, nullptr, 1, &top_notes).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "embedding top-k should require query values");
  require_true(
      kernel_query_ai_embedding_top_notes(handle, values, 0, nullptr, 1, &top_notes).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "embedding top-k should reject empty query vectors");
  require_true(
      kernel_query_ai_embedding_top_notes(handle, values, 1, nullptr, 0, &top_notes).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "embedding top-k should reject zero limit");
  require_true(
      kernel_query_ai_embedding_top_notes(handle, values, 1, nullptr, 1, nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "embedding top-k should require an output pointer");

  uint8_t deleted = 0;
  require_true(
      kernel_delete_ai_embedding_note(nullptr, "alpha.md", &deleted).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "embedding delete should require a handle");
  require_true(
      kernel_delete_ai_embedding_note(handle, "", &deleted).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "embedding delete should require a rel_path");
  require_true(
      kernel_delete_ai_embedding_note(handle, "alpha.md", nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "embedding delete should require an output flag");
  require_true(
      kernel_clear_ai_embeddings(nullptr).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "embedding clear should require a handle");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_ai_embedding_cache_tests() {
  test_embedding_cache_metadata_vectors_and_top_notes_are_kernel_owned();
  test_embedding_refresh_jobs_are_prepared_in_kernel();
  test_changed_embedding_refresh_jobs_refresh_catalog_in_kernel();
  test_embedding_cache_rejects_invalid_arguments();
}
