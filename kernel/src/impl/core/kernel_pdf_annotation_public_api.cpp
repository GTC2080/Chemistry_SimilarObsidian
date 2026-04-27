// Reason: This file owns stateless PDF annotation hashing rules so hosts do not
// duplicate storage-key or lightweight content-hash truth.

#include "kernel/c_api.h"

#include "core/kernel_shared.h"
#include "vault/revision.h"

#include <cstdint>
#include <cstring>
#include <new>
#include <string>
#include <string_view>

namespace {

constexpr std::size_t kAnnotationStorageKeyLength = 16;

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

}  // namespace

extern "C" kernel_status kernel_compute_pdf_annotation_storage_key(
    const char* pdf_path,
    kernel_owned_buffer* out_buffer) {
  if (out_buffer == nullptr || kernel::core::is_null_or_empty(pdf_path)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  const std::string digest = kernel::vault::sha256_hex(pdf_path);
  if (!fill_owned_buffer(
          std::string_view(digest).substr(0, kAnnotationStorageKeyLength),
          out_buffer)) {
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
