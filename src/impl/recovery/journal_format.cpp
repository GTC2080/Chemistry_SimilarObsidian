// Reason: This file owns framed journal record encoding and append primitives.

#include "recovery/journal.h"

#include "recovery/journal_internal.h"

#include "platform/platform.h"

#include <chrono>
#include <sstream>

namespace kernel::recovery::detail {

std::uint64_t now_ns() {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count());
}

std::string json_escape(std::string_view value) {
  std::string escaped;
  escaped.reserve(value.size());
  for (const char ch : value) {
    switch (ch) {
      case '\\':
        escaped += "\\\\";
        break;
      case '"':
        escaped += "\\\"";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        escaped += ch;
        break;
    }
  }
  return escaped;
}

std::uint32_t crc32(std::string_view bytes) {
  std::uint32_t crc = 0xffffffffu;
  for (const unsigned char byte : bytes) {
    crc ^= byte;
    for (int i = 0; i < 8; ++i) {
      const std::uint32_t mask = (crc & 1u) ? 0xffffffffu : 0u;
      crc = (crc >> 1) ^ (0xedb88320u & mask);
    }
  }
  return ~crc;
}

void append_u32_le(std::string& out, const std::uint32_t value) {
  out.push_back(static_cast<char>(value & 0xffu));
  out.push_back(static_cast<char>((value >> 8) & 0xffu));
  out.push_back(static_cast<char>((value >> 16) & 0xffu));
  out.push_back(static_cast<char>((value >> 24) & 0xffu));
}

std::uint32_t read_u32_le(std::string_view bytes, const std::size_t offset) {
  return static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset])) |
         (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset + 1])) << 8) |
         (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset + 2])) << 16) |
         (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset + 3])) << 24);
}

std::string make_payload(
    const std::string& operation_id,
    std::string_view phase,
    const std::string& rel_path,
    std::string_view temp_path) {
  std::ostringstream stream;
  stream << "{\"op_id\":\"" << json_escape(operation_id)
         << "\",\"op_type\":\"SAVE\""
         << ",\"phase\":\"" << phase
         << "\",\"rel_path\":\"" << json_escape(rel_path)
         << "\",\"temp_path\":\"" << json_escape(temp_path)
         << "\",\"recorded_at_ns\":" << now_ns()
         << "}";
  return stream.str();
}

std::string encode_record_frame(
    const std::string& operation_id,
    std::string_view phase,
    const std::string& rel_path,
    std::string_view temp_path) {
  const std::string payload = make_payload(operation_id, phase, rel_path, temp_path);

  std::string frame;
  frame.reserve(kMagic.size() + 4 + payload.size() + 4);
  frame.append(kMagic.data(), kMagic.size());
  append_u32_le(frame, static_cast<std::uint32_t>(payload.size()));
  frame.append(payload);
  append_u32_le(frame, crc32(payload));
  return frame;
}

std::error_code append_record(
    const std::filesystem::path& journal_path,
    const std::string& operation_id,
    std::string_view phase,
    const std::string& rel_path,
    std::string_view temp_path) {
  return kernel::platform::append_file_frame(
      journal_path,
      encode_record_frame(operation_id, phase, rel_path, temp_path));
}

}  // namespace kernel::recovery::detail

namespace kernel::recovery {

std::string make_operation_id() {
  return "op-" + std::to_string(detail::now_ns());
}

std::error_code append_save_begin(
    const std::filesystem::path& journal_path,
    const std::string& operation_id,
    const std::string& rel_path,
    const std::filesystem::path& temp_path) {
  return detail::append_record(journal_path, operation_id, "SAVE_BEGIN", rel_path, temp_path.generic_string());
}

std::error_code append_save_commit(
    const std::filesystem::path& journal_path,
    const std::string& operation_id,
    const std::string& rel_path) {
  return detail::append_record(journal_path, operation_id, "SAVE_COMMIT", rel_path, "");
}

}  // namespace kernel::recovery
