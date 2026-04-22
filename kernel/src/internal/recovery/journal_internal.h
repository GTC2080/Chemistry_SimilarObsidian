// Reason: This file centralizes private helpers shared by the split journal append and recovery units.

#pragma once

#include "storage/storage.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <vector>

namespace kernel::recovery::detail {

inline constexpr std::string_view kMagic = "KRJ1";

std::uint64_t now_ns();
std::string json_escape(std::string_view value);
std::uint32_t crc32(std::string_view bytes);
void append_u32_le(std::string& out, std::uint32_t value);
std::uint32_t read_u32_le(std::string_view bytes, std::size_t offset);

std::string make_payload(
    const std::string& operation_id,
    std::string_view phase,
    const std::string& rel_path,
    std::string_view temp_path);
std::string encode_record_frame(
    const std::string& operation_id,
    std::string_view phase,
    const std::string& rel_path,
    std::string_view temp_path);

struct JournalRecord {
  std::string op_id;
  std::string phase;
  std::string rel_path;
  std::string temp_path;
};

struct PendingSave {
  std::string rel_path;
  std::filesystem::path temp_path;
};

struct JournalScan {
  std::vector<JournalRecord> records;
  std::unordered_map<std::string, PendingSave> unfinished_ops;
  bool detected_corrupt_tail = false;
};

bool extract_json_string(std::string_view payload, std::string_view key, std::string& out_value);
std::error_code append_record(
    const std::filesystem::path& journal_path,
    const std::string& operation_id,
    std::string_view phase,
    const std::string& rel_path,
    std::string_view temp_path);
std::error_code read_journal_records(
    const std::filesystem::path& journal_path,
    std::vector<JournalRecord>& out_records,
    bool& out_detected_corrupt_tail);
void collect_unfinished_ops(
    const std::vector<JournalRecord>& records,
    std::unordered_map<std::string, PendingSave>& out_unfinished_ops);
std::error_code scan_journal(
    const std::filesystem::path& journal_path,
    JournalScan& out_scan);
std::error_code rewrite_journal(
    const std::filesystem::path& journal_path,
    const std::vector<JournalRecord>& records);
std::error_code recover_pending_save(
    const std::filesystem::path& vault_root,
    const PendingSave& pending_save,
    kernel::storage::Database& storage,
    bool* out_recovered_live_note = nullptr);

}  // namespace kernel::recovery::detail
