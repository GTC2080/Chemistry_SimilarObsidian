// Reason: This file holds API-test-only helpers so kernel_api_tests.cpp can focus on behavior groups.

#include "api/kernel_api_test_support.h"

#include "support/test_support.h"

#include <cstdint>
#include <fstream>
#include <vector>

std::uint32_t read_u32_le(const std::string& bytes, const std::size_t offset) {
  return static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset])) |
         (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset + 1])) << 8) |
         (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset + 2])) << 16) |
         (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset + 3])) << 24);
}

void append_u32_le(std::string& bytes, const std::uint32_t value) {
  bytes.push_back(static_cast<char>(value & 0xffu));
  bytes.push_back(static_cast<char>((value >> 8) & 0xffu));
  bytes.push_back(static_cast<char>((value >> 16) & 0xffu));
  bytes.push_back(static_cast<char>((value >> 24) & 0xffu));
}

std::vector<std::string> read_journal_payloads(const std::filesystem::path& journal_path) {
  std::ifstream input(journal_path, std::ios::binary);
  require_true(input.good(), "recovery journal must be readable");

  const std::string bytes{
      std::istreambuf_iterator<char>(input),
      std::istreambuf_iterator<char>()};

  std::vector<std::string> payloads;
  std::size_t offset = 0;
  while (offset + 12 <= bytes.size()) {
    require_true(bytes.compare(offset, 4, "KRJ1") == 0, "journal record magic must match");
    const std::uint32_t payload_size = read_u32_le(bytes, offset + 4);
    const std::size_t payload_offset = offset + 8;
    require_true(payload_offset + payload_size + 4 <= bytes.size(), "journal record must fit in file");
    payloads.emplace_back(bytes.substr(payload_offset, payload_size));
    offset = payload_offset + payload_size + 4;
  }

  require_true(offset == bytes.size(), "journal file must end on record boundary");
  return payloads;
}

void prepare_state_dir_for_vault(const std::filesystem::path& vault) {
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  expect_ok(kernel_close(handle));
}

void expect_empty_journal_if_present(const std::filesystem::path& journal_path, std::string_view message) {
  if (std::filesystem::exists(journal_path)) {
    const auto payloads = read_journal_payloads(journal_path);
    require_true(payloads.empty(), message);
  }
}

void append_truncated_tail_record(const std::filesystem::path& path) {
  std::string bytes = "KRJ1";
  append_u32_le(bytes, 9);
  bytes.append("{\"phase\":");
  append_raw_bytes(path, bytes);
}

void append_crc_mismatch_tail_record(const std::filesystem::path& path) {
  std::string payload = "{\"op_id\":\"bad\",\"phase\":\"SAVE_BEGIN\",\"rel_path\":\"bad.md\",\"temp_path\":\"bad.tmp\"}";
  std::string frame = "KRJ1";
  append_u32_le(frame, static_cast<std::uint32_t>(payload.size()));
  frame.append(payload);
  append_u32_le(frame, 0xdeadbeefu);
  append_raw_bytes(path, frame);
}

void require_index_ready(kernel_handle* handle, std::string_view message) {
  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_READY;
      },
      message);
}

void require_attachment_lookup_state(
    kernel_handle* handle,
    const char* attachment_rel_path,
    const kernel_attachment_presence expected_presence,
    const std::uint64_t expected_ref_count,
    const kernel_attachment_kind expected_kind,
    const bool expect_nonzero_metadata,
    const std::string_view context) {
  kernel_attachment_record attachment{};
  const kernel_status status = kernel_get_attachment(handle, attachment_rel_path, &attachment);
  require_true(
      status.code == KERNEL_OK,
      std::string(context) + ": formal attachment lookup should succeed, got status " +
          std::to_string(status.code));
  require_true(
      std::string(attachment.rel_path) == attachment_rel_path,
      std::string(context) + ": formal attachment lookup should preserve rel_path");
  require_true(
      attachment.presence == expected_presence,
      std::string(context) + ": formal attachment lookup should preserve presence");
  require_true(
      attachment.ref_count == expected_ref_count,
      std::string(context) + ": formal attachment lookup should preserve live ref_count");
  require_true(
      attachment.kind == expected_kind,
      std::string(context) + ": formal attachment lookup should preserve attachment kind");
  if (expect_nonzero_metadata) {
    require_true(
        attachment.file_size > 0,
        std::string(context) + ": formal attachment lookup should preserve non-zero file_size");
    require_true(
        attachment.mtime_ns > 0,
        std::string(context) + ": formal attachment lookup should preserve non-zero mtime_ns");
  }
  kernel_free_attachment_record(&attachment);
}

void require_single_note_attachment_ref_state(
    kernel_handle* handle,
    const char* note_rel_path,
    const char* attachment_rel_path,
    const kernel_attachment_presence expected_presence,
    const std::uint64_t expected_ref_count,
    const kernel_attachment_kind expected_kind,
    const bool expect_nonzero_metadata,
    const std::string_view context) {
  kernel_attachment_list refs{};
  const kernel_status status =
      kernel_query_note_attachment_refs(handle, note_rel_path, static_cast<size_t>(-1), &refs);
  require_true(
      status.code == KERNEL_OK,
      std::string(context) + ": formal note attachment refs query should succeed, got status " +
          std::to_string(status.code));
  require_true(
      refs.count == 1,
      std::string(context) + ": formal note attachment refs should expose exactly one live attachment");
  require_true(
      std::string(refs.attachments[0].rel_path) == attachment_rel_path,
      std::string(context) + ": formal note attachment refs should expose the expected rel_path");
  require_true(
      refs.attachments[0].presence == expected_presence,
      std::string(context) + ": formal note attachment refs should preserve presence");
  require_true(
      refs.attachments[0].ref_count == expected_ref_count,
      std::string(context) + ": formal note attachment refs should preserve live ref_count");
  require_true(
      refs.attachments[0].kind == expected_kind,
      std::string(context) + ": formal note attachment refs should preserve attachment kind");
  if (expect_nonzero_metadata) {
    require_true(
        refs.attachments[0].file_size > 0,
        std::string(context) + ": formal note attachment refs should preserve non-zero file_size");
    require_true(
        refs.attachments[0].mtime_ns > 0,
        std::string(context) + ": formal note attachment refs should preserve non-zero mtime_ns");
  }
  kernel_free_attachment_list(&refs);
}

void require_note_attachment_refs_not_found(
    kernel_handle* handle,
    const char* note_rel_path,
    const std::string_view context) {
  kernel_attachment_list refs{};
  const kernel_status status =
      kernel_query_note_attachment_refs(handle, note_rel_path, static_cast<size_t>(-1), &refs);
  require_true(
      status.code == KERNEL_ERROR_NOT_FOUND,
      std::string(context) + ": formal note attachment refs query should return NOT_FOUND, got status " +
          std::to_string(status.code));
  require_true(
      refs.attachments == nullptr && refs.count == 0,
      std::string(context) + ": formal note attachment refs query should clear stale output on NOT_FOUND");
}

void require_attachment_lookup_not_found(
    kernel_handle* handle,
    const char* attachment_rel_path,
    const std::string_view context) {
  kernel_attachment_record attachment{};
  const kernel_status status = kernel_get_attachment(handle, attachment_rel_path, &attachment);
  require_true(
      status.code == KERNEL_ERROR_NOT_FOUND,
      std::string(context) + ": formal attachment lookup should return NOT_FOUND, got status " +
          std::to_string(status.code));
  require_true(
      attachment.rel_path == nullptr && attachment.basename == nullptr &&
          attachment.extension == nullptr && attachment.ref_count == 0,
      std::string(context) + ": formal attachment lookup should clear stale output on NOT_FOUND");
}

void require_attachment_referrers_not_found(
    kernel_handle* handle,
    const char* attachment_rel_path,
    const std::string_view context) {
  kernel_attachment_referrers referrers{};
  const kernel_status status =
      kernel_query_attachment_referrers(handle, attachment_rel_path, static_cast<size_t>(-1), &referrers);
  require_true(
      status.code == KERNEL_ERROR_NOT_FOUND,
      std::string(context) + ": formal attachment referrers query should return NOT_FOUND, got status " +
          std::to_string(status.code));
  require_true(
      referrers.referrers == nullptr && referrers.count == 0,
      std::string(context) + ": formal attachment referrers query should clear stale output on NOT_FOUND");
}
