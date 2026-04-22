// Reason: Keep startup recovery tail-corruption scenarios together so broader cleanup tests can stay smaller.

#include "kernel/c_api.h"

#include "api/kernel_api_runtime_recovery_startup_suites.h"
#include "api/kernel_api_test_support.h"
#include "recovery/journal.h"
#include "support/test_support.h"

#include <filesystem>

namespace {

void test_open_vault_ignores_torn_tail_after_valid_recovery_prefix() {
  const auto vault = make_temp_vault();
  const auto state_dir = state_dir_for_vault(vault);
  const auto journal_path = journal_path_for_vault(vault);
  const auto target_path = vault / "tail.md";
  const auto temp_path = target_path.parent_path() / "tail.md.codex-recovery.tmp";

  prepare_state_dir_for_vault(vault);
  write_file_bytes(target_path, "tail-body");
  write_file_bytes(temp_path, "tail-temp");

  require_true(
      kernel::recovery::append_save_begin(
          journal_path,
          "tail-op",
          "tail.md",
          temp_path)
              .value() == 0,
      "valid SAVE_BEGIN append should succeed before torn tail injection");
  append_raw_bytes(journal_path, "BROKEN");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.pending_recovery_ops == 0 &&
               snapshot.index_state == KERNEL_INDEX_READY &&
               snapshot.indexed_note_count == 1;
      },
      "valid prefix recovery should settle to READY after torn-tail cleanup");
  require_true(!std::filesystem::exists(temp_path), "torn-tail recovery should still remove stale temp");

  expect_empty_journal_if_present(journal_path, "torn-tail recovery should compact journal to empty valid prefix");

  sqlite3* db = open_sqlite_readonly(storage_db_for_vault(vault));
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM notes WHERE rel_path='tail.md' AND is_deleted=0;") == 1,
      "valid prefix recovery should persist the recovered note");
  sqlite3_close(db);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir);
}

void test_open_vault_ignores_truncated_tail_after_valid_recovery_prefix() {
  const auto vault = make_temp_vault();
  const auto state_dir = state_dir_for_vault(vault);
  const auto journal_path = journal_path_for_vault(vault);
  const auto target_path = vault / "truncated.md";
  const auto temp_path = target_path.parent_path() / "truncated.md.codex-recovery.tmp";

  prepare_state_dir_for_vault(vault);
  write_file_bytes(target_path, "truncated-body");
  write_file_bytes(temp_path, "truncated-temp");

  require_true(
      kernel::recovery::append_save_begin(
          journal_path,
          "truncated-op",
          "truncated.md",
          temp_path)
              .value() == 0,
      "valid SAVE_BEGIN append should succeed before truncated tail injection");
  append_truncated_tail_record(journal_path);

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.pending_recovery_ops == 0 &&
               snapshot.index_state == KERNEL_INDEX_READY &&
               snapshot.indexed_note_count == 1;
      },
      "valid prefix recovery should settle to READY after truncated-tail cleanup");
  require_true(!std::filesystem::exists(temp_path), "truncated-tail recovery should still remove stale temp");

  expect_empty_journal_if_present(
      journal_path,
      "truncated-tail recovery should compact journal to empty valid prefix");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir);
}

void test_open_vault_ignores_crc_mismatch_tail_after_valid_recovery_prefix() {
  const auto vault = make_temp_vault();
  const auto state_dir = state_dir_for_vault(vault);
  const auto journal_path = journal_path_for_vault(vault);
  const auto target_path = vault / "crc.md";
  const auto temp_path = target_path.parent_path() / "crc.md.codex-recovery.tmp";

  prepare_state_dir_for_vault(vault);
  write_file_bytes(target_path, "crc-body");
  write_file_bytes(temp_path, "crc-temp");

  require_true(
      kernel::recovery::append_save_begin(
          journal_path,
          "crc-op",
          "crc.md",
          temp_path)
              .value() == 0,
      "valid SAVE_BEGIN append should succeed before CRC tail injection");
  append_crc_mismatch_tail_record(journal_path);

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.pending_recovery_ops == 0 &&
               snapshot.index_state == KERNEL_INDEX_READY &&
               snapshot.indexed_note_count == 1;
      },
      "valid prefix recovery should settle to READY after CRC-tail cleanup");
  require_true(!std::filesystem::exists(temp_path), "crc mismatch recovery should still remove stale temp");

  expect_empty_journal_if_present(journal_path, "crc mismatch recovery should compact journal to empty valid prefix");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir);
}

}  // namespace

void run_runtime_recovery_startup_tail_tests() {
  test_open_vault_ignores_torn_tail_after_valid_recovery_prefix();
  test_open_vault_ignores_truncated_tail_after_valid_recovery_prefix();
  test_open_vault_ignores_crc_mismatch_tail_after_valid_recovery_prefix();
}
