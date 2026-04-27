// Reason: This file verifies stateless PDF annotation hashing rules exposed by
// the kernel so hosts do not duplicate path-key or lightweight content-hash logic.

#include "kernel/c_api.h"

#include "api/kernel_api_pdf_surface_suites.h"
#include "support/test_support.h"

#include <cstdint>
#include <string>

namespace {

std::string take_buffer(kernel_owned_buffer* buffer) {
  const std::string value(
      buffer->data == nullptr ? "" : std::string(buffer->data, buffer->size));
  kernel_free_buffer(buffer);
  return value;
}

void test_pdf_annotation_storage_key_uses_kernel_sha256_prefix() {
  kernel_owned_buffer buffer{};
  expect_ok(kernel_compute_pdf_annotation_storage_key("assets/paper.pdf", &buffer));
  require_true(
      take_buffer(&buffer) == "d7af56fa7308eb53",
      "PDF annotation storage key should be first 16 hex chars of path SHA-256");

  require_true(
      kernel_compute_pdf_annotation_storage_key("", &buffer).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "PDF annotation storage key should reject empty path");
  require_true(
      kernel_compute_pdf_annotation_storage_key(nullptr, &buffer).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "PDF annotation storage key should reject null path");
  require_true(
      kernel_compute_pdf_annotation_storage_key("assets/paper.pdf", nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "PDF annotation storage key should reject null output");
}

void test_pdf_lightweight_hash_uses_head_tail_and_size() {
  const std::uint8_t head[] = {'%', 'P', 'D', 'F'};
  const std::uint8_t tail[] = {'E', 'O', 'F'};
  kernel_owned_buffer buffer{};
  expect_ok(kernel_compute_pdf_lightweight_hash(head, 4, tail, 3, 1234, &buffer));
  require_true(
      take_buffer(&buffer) ==
          "d1a340980bc6729a17b938075e8d855ebb53f367c78d49b6bd1040254c4ba5ca",
      "PDF lightweight hash should hash head, tail, and little-endian file size");

  require_true(
      kernel_compute_pdf_lightweight_hash(nullptr, 1, tail, 3, 1234, &buffer).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "PDF lightweight hash should reject null head when head size is nonzero");
  require_true(
      kernel_compute_pdf_lightweight_hash(head, 4, nullptr, 1, 1234, &buffer).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "PDF lightweight hash should reject null tail when tail size is nonzero");
  require_true(
      kernel_compute_pdf_lightweight_hash(head, 4, tail, 3, 1234, nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "PDF lightweight hash should reject null output");
}

}  // namespace

void run_pdf_surface_annotation_tests() {
  test_pdf_annotation_storage_key_uses_kernel_sha256_prefix();
  test_pdf_lightweight_hash_uses_head_tail_and_size();
}
