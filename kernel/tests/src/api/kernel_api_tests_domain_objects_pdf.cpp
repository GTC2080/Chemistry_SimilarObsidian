// Reason: Keep Track 4 PDF-adjacent subtype coverage separate so the new
// object surface can evolve without bloating metadata suites.

#include "kernel/c_api.h"

#include "api/kernel_api_pdf_test_helpers.h"
#include "api/kernel_api_domain_object_suites.h"
#include "support/test_support.h"

#include <filesystem>
#include <string>

namespace {

void test_pdf_domain_object_surface_exposes_canonical_pdf_subtype_states() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "assets");
  write_file_bytes(
      vault / "assets" / "ready.pdf",
      make_metadata_pdf_bytes(2, true, true, "/Title (Ready PDF) "));
  write_file_bytes(vault / "assets" / "invalid.pdf", "not-a-pdf");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string note =
      "# Domain Object PDF\n"
      "[Ready](assets/ready.pdf)\n"
      "[Invalid](assets/invalid.pdf)\n"
      "[Missing](assets/missing.pdf)\n";
  expect_ok(kernel_write_note(
      handle,
      "domain-object-pdf.md",
      note.data(),
      note.size(),
      nullptr,
      &metadata,
      &disposition));

  kernel_domain_object_list objects{};
  expect_ok(kernel_query_pdf_domain_objects(
      handle,
      "assets/ready.pdf",
      static_cast<size_t>(-1),
      &objects));
  require_true(objects.count == 1, "pdf subtype surface should expose one generic PDF subtype");
  require_true(
      std::string(objects.objects[0].domain_object_key) ==
          "dom:v1/pdf/assets%2Fready.pdf/generic/pdf_document",
      "pdf subtype surface should preserve canonical domain_object_key grammar");
  require_true(
      objects.objects[0].carrier_kind == KERNEL_DOMAIN_CARRIER_PDF,
      "pdf subtype surface should preserve pdf carrier kind");
  require_true(
      std::string(objects.objects[0].carrier_key) == "assets/ready.pdf",
      "pdf subtype surface should preserve normalized carrier_key");
  require_true(
      std::string(objects.objects[0].subtype_namespace) == "generic" &&
          std::string(objects.objects[0].subtype_name) == "pdf_document",
      "pdf subtype surface should preserve subtype namespace and name");
  require_true(
      objects.objects[0].coarse_kind == KERNEL_ATTACHMENT_KIND_PDF_LIKE,
      "pdf subtype surface should preserve coarse pdf kind");
  require_true(
      objects.objects[0].presence == KERNEL_ATTACHMENT_PRESENCE_PRESENT &&
          objects.objects[0].state == KERNEL_DOMAIN_OBJECT_PRESENT,
      "pdf subtype surface should report present state for ready pdf carriers");
  kernel_free_domain_object_list(&objects);

  expect_ok(kernel_query_pdf_domain_objects(
      handle,
      "assets/invalid.pdf",
      static_cast<size_t>(-1),
      &objects));
  require_true(
      objects.count == 1 &&
          objects.objects[0].state == KERNEL_DOMAIN_OBJECT_UNRESOLVED,
      "pdf subtype surface should report unresolved state for invalid pdf carriers");
  const std::string invalid_domain_object_key = objects.objects[0].domain_object_key;
  kernel_free_domain_object_list(&objects);

  expect_ok(kernel_query_pdf_domain_objects(
      handle,
      "assets/missing.pdf",
      static_cast<size_t>(-1),
      &objects));
  require_true(
      objects.count == 1 &&
          objects.objects[0].presence == KERNEL_ATTACHMENT_PRESENCE_MISSING &&
          objects.objects[0].state == KERNEL_DOMAIN_OBJECT_MISSING,
      "pdf subtype surface should report missing state for missing live pdf carriers");
  kernel_free_domain_object_list(&objects);

  kernel_domain_object_descriptor object{};
  expect_ok(kernel_get_domain_object(handle, invalid_domain_object_key.c_str(), &object));
  require_true(
      object.state == KERNEL_DOMAIN_OBJECT_UNRESOLVED,
      "single domain object lookup should preserve unresolved pdf subtype state");
  kernel_free_domain_object_descriptor(&object);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_pdf_domain_object_surface_rejects_invalid_or_non_live_keys() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "assets");
  write_file_bytes(
      vault / "assets" / "unreferenced.pdf",
      make_metadata_pdf_bytes(1, false, true, "/Title (U) "));
  write_file_bytes(vault / "assets" / "plain.png", "png-bytes");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_domain_object_list objects{};
  require_true(
      kernel_query_pdf_domain_objects(handle, "", static_cast<size_t>(-1), &objects).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "pdf subtype surface should reject empty attachment path");
  require_true(
      objects.objects == nullptr && objects.count == 0,
      "pdf subtype surface should clear stale output on invalid input");
  require_true(
      kernel_query_pdf_domain_objects(
          handle,
          "assets/unreferenced.pdf",
          static_cast<size_t>(-1),
          &objects)
              .code == KERNEL_ERROR_NOT_FOUND,
      "pdf subtype surface should reject unreferenced disk pdfs");
  require_true(
      kernel_query_pdf_domain_objects(handle, "assets/plain.png", static_cast<size_t>(-1), &objects)
              .code == KERNEL_ERROR_NOT_FOUND,
      "pdf subtype surface should reject non-pdf live attachments");
  require_true(
      objects.objects == nullptr && objects.count == 0,
      "pdf subtype surface should clear stale output on NOT_FOUND");

  kernel_domain_object_descriptor object{};
  require_true(
      kernel_get_domain_object(
          handle,
          "dom:v1/pdf/assets%2Funreferenced.pdf/generic/pdf_document",
          &object)
              .code == KERNEL_ERROR_NOT_FOUND,
      "single domain object lookup should reject non-live pdf carriers");
  require_true(
      kernel_get_domain_object(
          handle,
          "dom:v1/pdf/assets/ready.pdf/generic/pdf_document",
          &object)
              .code == KERNEL_ERROR_INVALID_ARGUMENT,
      "single domain object lookup should reject unescaped carrier keys");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_domain_object_pdf_tests() {
  test_pdf_domain_object_surface_exposes_canonical_pdf_subtype_states();
  test_pdf_domain_object_surface_rejects_invalid_or_non_live_keys();
}
