// Reason: This file translates ReadDirectoryChangesW FILE_NOTIFY_INFORMATION buffers into the kernel raw watcher model.

#include "watcher/windows_decode.h"

#include <Windows.h>

#include <string>

namespace kernel::watcher {
namespace {

RawChangeKind map_action(const DWORD action) {
  switch (action) {
    case FILE_ACTION_ADDED:
      return RawChangeKind::Created;
    case FILE_ACTION_REMOVED:
      return RawChangeKind::Deleted;
    case FILE_ACTION_MODIFIED:
      return RawChangeKind::Modified;
    case FILE_ACTION_RENAMED_OLD_NAME:
      return RawChangeKind::RenamedOld;
    case FILE_ACTION_RENAMED_NEW_NAME:
      return RawChangeKind::RenamedNew;
    default:
      return RawChangeKind::Modified;
  }
}

std::string narrow_path(const wchar_t* file_name, const DWORD file_name_bytes) {
  const auto wchar_count = static_cast<int>(file_name_bytes / sizeof(wchar_t));
  if (wchar_count <= 0) {
    return {};
  }

  const int needed = WideCharToMultiByte(
      CP_UTF8,
      0,
      file_name,
      wchar_count,
      nullptr,
      0,
      nullptr,
      nullptr);
  if (needed <= 0) {
    return {};
  }

  std::string result(static_cast<std::size_t>(needed), '\0');
  WideCharToMultiByte(
      CP_UTF8,
      0,
      file_name,
      wchar_count,
      result.data(),
      needed,
      nullptr,
      nullptr);
  for (char& ch : result) {
    if (ch == '\\') {
      ch = '/';
    }
  }
  return result;
}

}  // namespace

std::vector<RawChangeEvent> decode_win32_notify_buffer(const std::byte* bytes, const std::size_t size) {
  std::vector<RawChangeEvent> events;
  if (bytes == nullptr || size < sizeof(FILE_NOTIFY_INFORMATION)) {
    return events;
  }

  std::size_t offset = 0;
  while (offset + sizeof(FILE_NOTIFY_INFORMATION) <= size) {
    const auto* record = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(bytes + offset);
    events.push_back(
        RawChangeEvent{
            map_action(record->Action),
            narrow_path(record->FileName, record->FileNameLength)});

    if (record->NextEntryOffset == 0) {
      break;
    }

    offset += record->NextEntryOffset;
    if (offset >= size) {
      break;
    }
  }

  return events;
}

RawChangeEvent make_overflow_event() {
  return {RawChangeKind::Overflow, ""};
}

}  // namespace kernel::watcher
