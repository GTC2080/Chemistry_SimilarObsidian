// Reason: Keep Track 4 attachment-subtype coverage separate so the new object
// surface can evolve without bloating metadata suites.

#include "kernel/c_api.h"

#include "api/kernel_api_domain_object_suites.h"
#include "support/test_support.h"

#include <filesystem>
#include <string>

namespace {

void test_attachment_domain_object_surface_exposes_canonical_generic_subtype() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "assets");
  write_file_bytes(vault / "assets" / "diagram.png", "png-bytes");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string note =
      "# Domain Object Attachment\n"
      "![Diagram](assets/diagram.png)\n"
      "[Chem](assets/missing.sdf)\n";
  expect_ok(kernel_write_note(
      handle,
      "domain-object-attachment.md",
      note.data(),
      note.size(),
      nullptr,
      &metadata,
      &disposition));

  kernel_domain_object_list objects{};
  expect_ok(kernel_query_attachment_domain_objects(
      handle,
      "assets/diagram.png",
      static_cast<size_t>(-1),
      &objects));
  require_true(objects.count == 1, "attachment subtype surface should expose one generic subtype");
  require_true(
      std::string(objects.objects[0].domain_object_key) ==
          "dom:v1/attachment/assets%2Fdiagram.png/generic/attachment_resource",
      "attachment subtype surface should preserve canonical domain_object_key grammar");
  require_true(
      objects.objects[0].carrier_kind == KERNEL_DOMAIN_CARRIER_ATTACHMENT,
      "attachment subtype surface should preserve attachment carrier kind");
  require_true(
      std::string(objects.objects[0].carrier_key) == "assets/diagram.png",
      "attachment subtype surface should preserve normalized carrier_key");
  require_true(
      std::string(objects.objects[0].subtype_namespace) == "generic" &&
          std::string(objects.objects[0].subtype_name) == "attachment_resource",
      "attachment subtype surface should preserve subtype namespace and name");
  require_true(
      objects.objects[0].subtype_revision == 1,
      "attachment subtype surface should preserve subtype revision");
  require_true(
      objects.objects[0].coarse_kind == KERNEL_ATTACHMENT_KIND_IMAGE_LIKE,
      "attachment subtype surface should preserve coarse attachment kind");
  require_true(
      objects.objects[0].presence == KERNEL_ATTACHMENT_PRESENCE_PRESENT,
      "attachment subtype surface should preserve carrier presence");
  require_true(
      objects.objects[0].state == KERNEL_DOMAIN_OBJECT_PRESENT,
      "attachment subtype surface should report present state for present carriers");
  kernel_free_domain_object_list(&objects);

  expect_ok(kernel_query_attachment_domain_objects(handle, "assets\\diagram.png", 1, &objects));
  require_true(
      objects.count == 1 &&
          std::string(objects.objects[0].carrier_key) == "assets/diagram.png",
      "attachment subtype surface should normalize backslash input");
  kernel_free_domain_object_list(&objects);

  expect_ok(kernel_query_attachment_domain_objects(
      handle,
      "assets/missing.sdf",
      static_cast<size_t>(-1),
      &objects));
  require_true(
      objects.count == 1 &&
          objects.objects[0].coarse_kind == KERNEL_ATTACHMENT_KIND_CHEM_LIKE,
      "attachment subtype surface should preserve chem-like coarse kind without welding chemistry schema");
  require_true(
      objects.objects[0].presence == KERNEL_ATTACHMENT_PRESENCE_MISSING &&
          objects.objects[0].state == KERNEL_DOMAIN_OBJECT_MISSING,
      "attachment subtype surface should report missing state for missing live carriers");
  const std::string domain_object_key = objects.objects[0].domain_object_key;
  kernel_free_domain_object_list(&objects);

  kernel_domain_object_descriptor object{};
  expect_ok(kernel_get_domain_object(handle, domain_object_key.c_str(), &object));
  require_true(
      std::string(object.domain_object_key) == domain_object_key,
      "single domain object lookup should roundtrip canonical domain_object_key");
  require_true(
      object.state == KERNEL_DOMAIN_OBJECT_MISSING,
      "single domain object lookup should preserve missing state");
  kernel_free_domain_object_descriptor(&object);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_attachment_domain_object_surface_rejects_invalid_or_non_live_keys() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "assets");
  write_file_bytes(vault / "assets" / "unreferenced.png", "png-bytes");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_domain_object_list objects{};
  require_true(
      kernel_query_attachment_domain_objects(handle, "", static_cast<size_t>(-1), &objects).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "attachment subtype surface should reject empty attachment path");
  require_true(
      objects.objects == nullptr && objects.count == 0,
      "attachment subtype surface should clear stale output on invalid input");
  require_true(
      kernel_query_attachment_domain_objects(
          handle,
          "assets/unreferenced.png",
          static_cast<size_t>(-1),
          &objects)
              .code == KERNEL_ERROR_NOT_FOUND,
      "attachment subtype surface should reject non-live carriers");
  require_true(
      objects.objects == nullptr && objects.count == 0,
      "attachment subtype surface should clear stale output on NOT_FOUND");

  kernel_domain_object_descriptor object{};
  require_true(
      kernel_get_domain_object(handle, "", &object).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "single domain object lookup should reject empty key");
  require_true(
      kernel_get_domain_object(
          handle,
          "dom:v1/attachment/assets%2Funreferenced.png/generic/attachment_resource",
          &object)
              .code == KERNEL_ERROR_NOT_FOUND,
      "single domain object lookup should reject non-live carriers");
  require_true(
      kernel_get_domain_object(
          handle,
          "dom:v1/attachment/assets%2Fdiagram.png/generic/Attachment_Resource",
          &object)
              .code == KERNEL_ERROR_INVALID_ARGUMENT,
      "single domain object lookup should reject noncanonical subtype token casing");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_domain_object_attachment_tests() {
  test_attachment_domain_object_surface_exposes_canonical_generic_subtype();
  test_attachment_domain_object_surface_rejects_invalid_or_non_live_keys();
}
