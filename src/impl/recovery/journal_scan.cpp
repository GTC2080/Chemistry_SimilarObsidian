// Reason: This file owns framed journal scanning, unfinished-op collection, and compaction rewrite.

#include "recovery/journal_internal.h"

#include "platform/platform.h"

#include <string>

namespace kernel::recovery::detail {

bool extract_json_string(std::string_view payload, std::string_view key, std::string& out_value) {
  const std::string pattern = "\"" + std::string(key) + "\":\"";
  const std::size_t key_pos = payload.find(pattern);
  if (key_pos == std::string_view::npos) {
    return false;
  }

  std::size_t cursor = key_pos + pattern.size();
  std::string value;
  while (cursor < payload.size()) {
    const char ch = payload[cursor++];
    if (ch == '\\') {
      if (cursor >= payload.size()) {
        return false;
      }
      const char escaped = payload[cursor++];
      switch (escaped) {
        case '\\':
        case '"':
          value.push_back(escaped);
          break;
        case 'n':
          value.push_back('\n');
          break;
        case 'r':
          value.push_back('\r');
          break;
        case 't':
          value.push_back('\t');
          break;
        default:
          return false;
      }
      continue;
    }

    if (ch == '"') {
      out_value = std::move(value);
      return true;
    }

    value.push_back(ch);
  }

  return false;
}

std::error_code read_journal_records(
    const std::filesystem::path& journal_path,
    std::vector<JournalRecord>& out_records,
    bool& out_detected_corrupt_tail) {
  out_records.clear();
  out_detected_corrupt_tail = false;

  kernel::platform::ReadFileResult file;
  const std::error_code read_ec = kernel::platform::read_file(journal_path, file);
  if (read_ec == std::errc::no_such_file_or_directory) {
    return {};
  }
  if (read_ec) {
    return read_ec;
  }

  std::size_t offset = 0;
  const std::string_view bytes(file.bytes);

  while (offset + 12 <= bytes.size()) {
    if (bytes.substr(offset, kMagic.size()) != kMagic) {
      break;
    }

    const std::uint32_t payload_size = read_u32_le(bytes, offset + 4);
    const std::size_t payload_offset = offset + 8;
    if (payload_offset + payload_size + 4 > bytes.size()) {
      break;
    }

    const std::string_view payload = bytes.substr(payload_offset, payload_size);
    const std::uint32_t expected_crc = read_u32_le(bytes, payload_offset + payload_size);
    if (crc32(payload) != expected_crc) {
      break;
    }

    JournalRecord record;
    if (!extract_json_string(payload, "op_id", record.op_id) ||
        !extract_json_string(payload, "phase", record.phase) ||
        !extract_json_string(payload, "rel_path", record.rel_path) ||
        !extract_json_string(payload, "temp_path", record.temp_path)) {
      break;
    }

    out_records.push_back(std::move(record));
    offset = payload_offset + payload_size + 4;
  }

  if (offset < bytes.size()) {
    out_detected_corrupt_tail = true;
  }

  return {};
}

void collect_unfinished_ops(
    const std::vector<JournalRecord>& records,
    std::unordered_map<std::string, PendingSave>& out_unfinished_ops) {
  out_unfinished_ops.clear();
  for (const auto& record : records) {
    if (record.phase == "SAVE_BEGIN") {
      out_unfinished_ops[record.op_id] = PendingSave{
          record.rel_path,
          std::filesystem::path(record.temp_path)};
    } else if (record.phase == "SAVE_COMMIT") {
      out_unfinished_ops.erase(record.op_id);
    }
  }
}

std::error_code scan_journal(const std::filesystem::path& journal_path, JournalScan& out_scan) {
  out_scan.records.clear();
  out_scan.unfinished_ops.clear();
  out_scan.detected_corrupt_tail = false;

  std::error_code ec =
      read_journal_records(journal_path, out_scan.records, out_scan.detected_corrupt_tail);
  if (ec) {
    return ec;
  }

  collect_unfinished_ops(out_scan.records, out_scan.unfinished_ops);
  return {};
}

std::error_code rewrite_journal(
    const std::filesystem::path& journal_path,
    const std::vector<JournalRecord>& records) {
  std::filesystem::path temp_path;
  std::string bytes;
  for (const auto& record : records) {
    bytes += encode_record_frame(record.op_id, record.phase, record.rel_path, record.temp_path);
  }

  std::error_code ec = kernel::platform::write_temp_file(journal_path, bytes, temp_path);
  if (ec) {
    return ec;
  }
  return kernel::platform::atomic_replace_file(temp_path, journal_path);
}

}  // namespace kernel::recovery::detail
