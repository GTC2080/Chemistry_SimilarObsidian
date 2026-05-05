// Reason: This file owns stateless PDF annotation hashing rules so hosts do not
// duplicate storage-key or lightweight content-hash truth.

#include "kernel/c_api.h"

#include "core/kernel_shared.h"
#include "vault/revision.h"

#include <cstdint>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <new>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

constexpr std::size_t kAnnotationStorageKeyLength = 16;

std::string annotation_storage_key(std::string_view pdf_path) {
  const std::string digest = kernel::vault::sha256_hex(pdf_path);
  return std::string(std::string_view(digest).substr(0, kAnnotationStorageKeyLength));
}

bool pdf_annotation_file_path(
    kernel_handle* handle,
    const char* pdf_rel_path,
    std::filesystem::path& out_path) {
  out_path.clear();
  if (handle == nullptr || !kernel::core::is_valid_relative_path(pdf_rel_path)) {
    return false;
  }
  const std::string normalized = kernel::core::normalize_rel_path(pdf_rel_path);
  out_path = handle->paths.root / ".nexus" / "pdf-annotations" /
      (annotation_storage_key(normalized) + ".json");
  return true;
}

bool fill_owned_buffer(std::string_view value, kernel_owned_buffer* out_buffer) {
  out_buffer->data = nullptr;
  out_buffer->size = 0;
  if (value.empty()) {
    return true;
  }

  auto* owned = new (std::nothrow) char[value.size()];
  if (owned == nullptr) {
    return false;
  }
  std::memcpy(owned, value.data(), value.size());
  out_buffer->data = owned;
  out_buffer->size = value.size();
  return true;
}

void append_u64_le(std::string& value, const std::uint64_t number) {
  for (int shift = 0; shift < 64; shift += 8) {
    value.push_back(static_cast<char>((number >> shift) & 0xffU));
  }
}

std::error_code read_pdf_hash_chunks(
    const std::filesystem::path& path,
    const std::uint64_t file_size,
    std::vector<std::uint8_t>& head,
    std::vector<std::uint8_t>& tail) {
  constexpr std::uint64_t kChunkSize = 1024;
  const std::size_t head_size =
      static_cast<std::size_t>(std::min<std::uint64_t>(kChunkSize, file_size));
  const std::uint64_t tail_offset = file_size > kChunkSize ? file_size - kChunkSize : 0;
  const std::size_t tail_size =
      static_cast<std::size_t>(std::min<std::uint64_t>(kChunkSize, file_size));

  head.assign(head_size, 0);
  tail.assign(tail_size, 0);

  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return std::make_error_code(std::errc::io_error);
  }

  if (head_size > 0) {
    input.seekg(0, std::ios::beg);
    if (!input) {
      return std::make_error_code(std::errc::io_error);
    }
    input.read(reinterpret_cast<char*>(head.data()), static_cast<std::streamsize>(head.size()));
    if (static_cast<std::size_t>(input.gcount()) != head.size()) {
      return std::make_error_code(std::errc::io_error);
    }
  }

  if (tail_size > 0) {
    input.clear();
    input.seekg(static_cast<std::streamoff>(tail_offset), std::ios::beg);
    if (!input) {
      return std::make_error_code(std::errc::io_error);
    }
    input.read(reinterpret_cast<char*>(tail.data()), static_cast<std::streamsize>(tail.size()));
    if (static_cast<std::size_t>(input.gcount()) != tail.size()) {
      return std::make_error_code(std::errc::io_error);
    }
  }

  return {};
}

}  // namespace

extern "C" kernel_status kernel_compute_pdf_annotation_storage_key(
    const char* pdf_path,
    kernel_owned_buffer* out_buffer) {
  if (out_buffer == nullptr || kernel::core::is_null_or_empty(pdf_path)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  if (!fill_owned_buffer(annotation_storage_key(pdf_path), out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_compute_pdf_lightweight_hash(
    const std::uint8_t* head,
    const std::size_t head_size,
    const std::uint8_t* tail,
    const std::size_t tail_size,
    const std::uint64_t file_size,
    kernel_owned_buffer* out_buffer) {
  if (
      out_buffer == nullptr || (head_size > 0 && head == nullptr) ||
      (tail_size > 0 && tail == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  std::string basis;
  basis.reserve(head_size + tail_size + sizeof(std::uint64_t));
  if (head_size > 0) {
    basis.append(reinterpret_cast<const char*>(head), head_size);
  }
  if (tail_size > 0) {
    basis.append(reinterpret_cast<const char*>(tail), tail_size);
  }
  append_u64_le(basis, file_size);

  const std::string digest = kernel::vault::sha256_hex(basis);
  if (!fill_owned_buffer(digest, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_compute_pdf_file_lightweight_hash(
    kernel_handle* handle,
    const char* host_path,
    const std::size_t host_path_size,
    kernel_owned_buffer* out_buffer) {
  if (handle == nullptr || out_buffer == nullptr || (host_path_size > 0 && host_path == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  kernel_owned_buffer rel_path_buffer{};
  const kernel_status rel_status =
      kernel_relativize_vault_path(handle, host_path, host_path_size, 0, &rel_path_buffer);
  if (rel_status.code != KERNEL_OK) {
    kernel_free_buffer(&rel_path_buffer);
    return rel_status;
  }

  const std::string rel_path(
      rel_path_buffer.data == nullptr ? "" : std::string(rel_path_buffer.data, rel_path_buffer.size));
  kernel_free_buffer(&rel_path_buffer);
  if (!kernel::core::is_valid_relative_path(rel_path.c_str())) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::filesystem::path target_path = kernel::core::resolve_note_path(handle, rel_path.c_str());
  kernel::platform::FileStat stat{};
  const std::error_code stat_ec = kernel::platform::stat_file(target_path, stat);
  if (stat_ec) {
    return kernel::core::make_status(kernel::core::map_error(stat_ec));
  }
  if (!stat.exists || !stat.is_regular_file) {
    return kernel::core::make_status(KERNEL_ERROR_NOT_FOUND);
  }

  std::vector<std::uint8_t> head;
  std::vector<std::uint8_t> tail;
  const std::error_code read_ec = read_pdf_hash_chunks(target_path, stat.file_size, head, tail);
  if (read_ec) {
    return kernel::core::make_status(kernel::core::map_error(read_ec));
  }

  return kernel_compute_pdf_lightweight_hash(
      head.empty() ? nullptr : head.data(),
      head.size(),
      tail.empty() ? nullptr : tail.data(),
      tail.size(),
      stat.file_size,
      out_buffer);
}

extern "C" kernel_status kernel_read_pdf_annotation_file(
    kernel_handle* handle,
    const char* pdf_rel_path,
    kernel_owned_buffer* out_buffer) {
  if (out_buffer == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  std::filesystem::path annotation_path;
  if (!pdf_annotation_file_path(handle, pdf_rel_path, annotation_path)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel::platform::FileStat stat{};
  const std::error_code stat_ec = kernel::platform::stat_file(annotation_path, stat);
  if (stat_ec) {
    return kernel::core::make_status(kernel::core::map_error(stat_ec));
  }
  if (!stat.exists) {
    return kernel::core::make_status(KERNEL_OK);
  }

  kernel::platform::ReadFileResult file{};
  const std::error_code read_ec = kernel::platform::read_file(annotation_path, file);
  if (read_ec) {
    return kernel::core::make_status(kernel::core::map_error(read_ec));
  }
  if (!fill_owned_buffer(file.bytes, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_write_pdf_annotation_file(
    kernel_handle* handle,
    const char* pdf_rel_path,
    const char* json_utf8,
    const std::size_t json_size) {
  if (json_size > 0 && json_utf8 == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::filesystem::path annotation_path;
  if (!pdf_annotation_file_path(handle, pdf_rel_path, annotation_path)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::filesystem::path temp_path;
  const std::string_view json(json_utf8 == nullptr ? "" : json_utf8, json_size);
  const std::error_code write_ec =
      kernel::platform::write_temp_file(annotation_path, json, temp_path);
  if (write_ec) {
    return kernel::core::make_status(kernel::core::map_error(write_ec));
  }

  const std::error_code replace_ec =
      kernel::platform::atomic_replace_file(temp_path, annotation_path);
  if (replace_ec) {
    kernel::platform::remove_file_if_exists(temp_path);
    return kernel::core::make_status(kernel::core::map_error(replace_ec));
  }
  return kernel::core::make_status(KERNEL_OK);
}
