// Reason: This file verifies stateless PDF annotation hashing rules exposed by
// the kernel so hosts do not duplicate path-key or lightweight content-hash logic.

#include "kernel/c_api.h"

#include "api/kernel_api_pdf_surface_suites.h"
#include "support/test_support.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

std::string take_buffer(kernel_owned_buffer* buffer) {
  const std::string value(
      buffer->data == nullptr ? "" : std::string(buffer->data, buffer->size));
  kernel_free_buffer(buffer);
  return value;
}

std::string read_text_file(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

void close_and_cleanup(kernel_handle* handle, const std::filesystem::path& vault) {
  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
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

void test_pdf_file_lightweight_hash_reads_vault_file_in_kernel() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "assets");

  std::string pdf_bytes(3000, 'x');
  pdf_bytes.replace(0, 4, "%PDF");
  pdf_bytes.replace(pdf_bytes.size() - 5, 5, "%%EOF");
  write_file_bytes(vault / "assets" / "paper.pdf", pdf_bytes);
  write_file_bytes(vault.parent_path() / "outside.pdf", "%PDF outside %%EOF");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_owned_buffer expected{};
  expect_ok(kernel_compute_pdf_lightweight_hash(
      reinterpret_cast<const std::uint8_t*>(pdf_bytes.data()),
      1024,
      reinterpret_cast<const std::uint8_t*>(pdf_bytes.data() + pdf_bytes.size() - 1024),
      1024,
      static_cast<std::uint64_t>(pdf_bytes.size()),
      &expected));

  const std::string pdf_path = (vault / "assets" / "paper.pdf").string();
  kernel_owned_buffer actual{};
  expect_ok(kernel_compute_pdf_file_lightweight_hash(
      handle,
      pdf_path.data(),
      pdf_path.size(),
      &actual));
  require_true(
      take_buffer(&actual) == take_buffer(&expected),
      "PDF file lightweight hash should read head and tail bytes from a vault file");

  const std::string outside_path = (vault.parent_path() / "outside.pdf").string();
  require_true(
      kernel_compute_pdf_file_lightweight_hash(
          handle,
          outside_path.data(),
          outside_path.size(),
          &actual).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "PDF file lightweight hash should reject paths outside the vault root");
  require_true(
      kernel_compute_pdf_file_lightweight_hash(handle, pdf_path.data(), pdf_path.size(), nullptr)
              .code == KERNEL_ERROR_INVALID_ARGUMENT,
      "PDF file lightweight hash should require output pointer");

  close_and_cleanup(handle, vault);
  std::filesystem::remove(vault.parent_path() / "outside.pdf");
}

void test_pdf_annotation_file_io_uses_kernel_storage_key() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string json =
      "{\n"
      "  \"pdfPath\": \"docs/Paper.pdf\",\n"
      "  \"pdfHash\": \"hash\",\n"
      "  \"annotations\": []\n"
      "}";
  expect_ok(kernel_write_pdf_annotation_file(
      handle,
      "docs/Paper.pdf",
      json.data(),
      json.size()));

  kernel_owned_buffer key{};
  expect_ok(kernel_compute_pdf_annotation_storage_key("docs/Paper.pdf", &key));
  const auto storage_path = vault / ".nexus" / "pdf-annotations" /
      (take_buffer(&key) + ".json");
  require_true(
      std::filesystem::exists(storage_path),
      "PDF annotation file should be stored under the kernel storage key");
  require_true(
      read_text_file(storage_path) == json,
      "PDF annotation write should preserve JSON bytes");

  kernel_owned_buffer loaded{};
  expect_ok(kernel_read_pdf_annotation_file(handle, "docs/Paper.pdf", &loaded));
  require_true(
      take_buffer(&loaded) == json,
      "PDF annotation read should return the stored JSON bytes");

  expect_ok(kernel_read_pdf_annotation_file(handle, "docs/Missing.pdf", &loaded));
  require_true(
      loaded.data == nullptr && loaded.size == 0,
      "missing PDF annotation file should read as an empty buffer");
  kernel_free_buffer(&loaded);

  require_true(
      kernel_read_pdf_annotation_file(handle, "../Paper.pdf", &loaded).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "PDF annotation read should reject invalid relative paths");
  require_true(
      kernel_read_pdf_annotation_file(handle, "docs/Paper.pdf", nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "PDF annotation read should require output pointer");
  require_true(
      kernel_write_pdf_annotation_file(handle, "docs/Paper.pdf", nullptr, 1).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "PDF annotation write should reject null non-empty JSON");

  close_and_cleanup(handle, vault);
}

}  // namespace

void run_pdf_surface_annotation_tests() {
  test_pdf_annotation_storage_key_uses_kernel_sha256_prefix();
  test_pdf_lightweight_hash_uses_head_tail_and_size();
  test_pdf_file_lightweight_hash_reads_vault_file_in_kernel();
  test_pdf_annotation_file_io_uses_kernel_storage_key();
}
