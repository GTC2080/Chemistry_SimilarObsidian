// Reason: Keep legacy attachment ABI regressions separate from the formal attachment public-surface suite.

#include "kernel/c_api.h"

#include "api/kernel_api_core_contract_suites.h"
#include "api/kernel_api_test_support.h"
#include "support/test_support.h"
#include "third_party/sqlite/sqlite3.h"

#include <filesystem>
#include <string>

namespace {

void test_write_persists_attachment_metadata_and_refs() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);
  std::filesystem::create_directories(vault / "assets");
  std::filesystem::create_directories(vault / "docs");
  write_file_bytes(vault / "assets" / "diagram.png", "diagram-bytes");
  write_file_bytes(vault / "docs" / "paper.pdf", "paper-bytes");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Attachment Title\n"
      "![Figure](assets/diagram.png)\n"
      "[Paper](docs/paper.pdf)\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "attachments.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));

  sqlite3* db = open_sqlite_readonly(db_path);
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM attachments WHERE rel_path='assets/diagram.png' AND is_missing=0;") == 1,
      "write should register present image attachment metadata");
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM attachments WHERE rel_path='docs/paper.pdf' AND is_missing=0;") == 1,
      "write should register present document attachment metadata");
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM note_attachment_refs "
          "WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='attachments.md');") == 2,
      "write should persist two note attachment refs");
  require_true(
      query_single_text(
          db,
          "SELECT attachment_rel_path FROM note_attachment_refs "
          "WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='attachments.md') "
          "ORDER BY rowid LIMIT 1;") == "assets/diagram.png",
      "attachment refs should preserve parser order in storage");
  sqlite3_close(db);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_rewrite_replaces_old_attachment_refs() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);
  std::filesystem::create_directories(vault / "assets");
  std::filesystem::create_directories(vault / "docs");
  write_file_bytes(vault / "assets" / "first.png", "first-bytes");
  write_file_bytes(vault / "docs" / "second.pdf", "second-bytes");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string original =
      "# Attachment Rewrite\n"
      "![First](assets/first.png)\n";
  kernel_note_metadata first{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "attachment-rewrite.md",
      original.data(),
      original.size(),
      nullptr,
      &first,
      &disposition));

  const std::string rewritten =
      "# Attachment Rewrite\n"
      "[Second](docs/second.pdf)\n";
  kernel_note_metadata second{};
  expect_ok(kernel_write_note(
      handle,
      "attachment-rewrite.md",
      rewritten.data(),
      rewritten.size(),
      first.content_revision,
      &second,
      &disposition));

  sqlite3* db = open_sqlite_readonly(db_path);
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM note_attachment_refs "
          "WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='attachment-rewrite.md');") == 1,
      "rewrite should clear old attachment refs before inserting new ones");
  require_true(
      query_single_text(
          db,
          "SELECT attachment_rel_path FROM note_attachment_refs "
          "WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='attachment-rewrite.md') LIMIT 1;") == "docs/second.pdf",
      "rewrite should persist only the new attachment ref set");
  sqlite3_close(db);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_attachment_api_lists_note_refs_in_parser_order() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "assets");
  std::filesystem::create_directories(vault / "docs");
  write_file_bytes(vault / "assets" / "diagram.png", "diagram-bytes");
  write_file_bytes(vault / "docs" / "paper.pdf", "paper-bytes");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Attachment API\n"
      "![Figure](assets/diagram.png)\n"
      "[Paper](docs/paper.pdf)\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "attachment-api.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));

  kernel_attachment_refs refs{};
  expect_ok(kernel_list_note_attachments(handle, "attachment-api.md", &refs));
  require_true(refs.count == 2, "attachment API should return two attachment refs");
  require_true(std::string(refs.refs[0].rel_path) == "assets/diagram.png", "attachment API should preserve parser order");
  require_true(std::string(refs.refs[1].rel_path) == "docs/paper.pdf", "attachment API should preserve parser order");
  kernel_free_attachment_refs(&refs);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_attachment_api_reports_missing_state_and_rejects_invalid_inputs() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "assets");
  std::filesystem::create_directories(vault / "docs");
  write_file_bytes(vault / "assets" / "present.png", "present-bytes");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Attachment Missing\n"
      "![Present](assets/present.png)\n"
      "![Missing](docs/missing.pdf)\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "attachment-missing.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));

  kernel_attachment_metadata attachment{};
  expect_ok(kernel_get_attachment_metadata(handle, "assets/present.png", &attachment));
  require_true(attachment.is_missing == 0, "present attachment metadata should report not missing");
  require_true(attachment.file_size > 0, "present attachment metadata should preserve file size");

  expect_ok(kernel_get_attachment_metadata(handle, "docs/missing.pdf", &attachment));
  require_true(attachment.is_missing == 1, "missing attachment metadata should report missing");

  require_true(
      kernel_list_note_attachments(handle, "", nullptr).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "attachment refs query should reject null output");
  kernel_attachment_refs refs{};
  require_true(
      kernel_list_note_attachments(handle, "", &refs).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "attachment refs query should reject empty note path");
  require_true(
      kernel_get_attachment_metadata(handle, "", &attachment).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "attachment metadata query should reject empty attachment path");
  require_true(
      kernel_get_attachment_metadata(handle, "..\\bad.bin", &attachment).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "attachment metadata query should reject path traversal");
  require_true(
      kernel_get_attachment_metadata(handle, "assets/unknown.bin", &attachment).code == KERNEL_ERROR_NOT_FOUND,
      "attachment metadata query should distinguish unknown paths from known missing rows");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_kernel_api_attachment_legacy_contract_tests() {
  test_write_persists_attachment_metadata_and_refs();
  test_rewrite_replaces_old_attachment_refs();
  test_attachment_api_lists_note_refs_in_parser_order();
  test_attachment_api_reports_missing_state_and_rejects_invalid_inputs();
}
