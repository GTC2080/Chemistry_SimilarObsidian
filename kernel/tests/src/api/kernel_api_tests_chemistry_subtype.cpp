// Reason: Keep Track 5 Batch 2 chemistry subtype coverage separate so the
// new spectrum object surface can evolve without bloating metadata suites.

#include "kernel/c_api.h"

#include "api/kernel_api_chemistry_suites.h"
#include "support/test_support.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace {

std::string make_jcamp_bytes(
    std::string_view title,
    std::string_view datatype,
    std::string_view xunits,
    std::string_view yunits,
    const std::uint64_t point_count) {
  return "##JCAMP-DX=5.01\n"
         "##TITLE=" + std::string(title) + "\n"
         "##DATA TYPE=" + std::string(datatype) + "\n"
         "##XUNITS=" + std::string(xunits) + "\n"
         "##YUNITS=" + std::string(yunits) + "\n"
         "##NPOINTS=" + std::to_string(point_count) + "\n";
}

std::string make_spectrum_csv(
    std::string_view x_unit,
    std::string_view y_unit,
    std::string_view family,
    std::string_view sample_label,
    std::string_view rows) {
  return "# x_unit=" + std::string(x_unit) + "\n" +
         "# y_unit=" + std::string(y_unit) + "\n" +
         "# family=" + std::string(family) + "\n" +
         "# sample_label=" + std::string(sample_label) + "\n" +
         "x,y\n" + std::string(rows);
}

void require_ok_status(const kernel_status status, std::string_view context) {
  require_true(
      status.code == KERNEL_OK,
      std::string(context) + ": expected KERNEL_OK, got status " +
          std::to_string(status.code));
}

const kernel_chem_spectrum_record* find_spectrum(
    const kernel_chem_spectrum_list& spectra,
    const char* rel_path) {
  for (size_t index = 0; index < spectra.count; ++index) {
    if (spectra.spectra[index].attachment_rel_path != nullptr &&
        std::string(spectra.spectra[index].attachment_rel_path) == rel_path) {
      return &spectra.spectra[index];
    }
  }
  return nullptr;
}

void require_spectrum_state(
    const kernel_chem_spectrum_list& spectra,
    const char* rel_path,
    const kernel_chem_spectrum_format expected_format,
    const kernel_attachment_kind expected_coarse_kind,
    const kernel_attachment_presence expected_presence,
    const kernel_domain_object_state expected_state,
    const char* expected_key,
    std::string_view context) {
  const auto* spectrum = find_spectrum(spectra, rel_path);
  require_true(spectrum != nullptr, std::string(context) + ": expected chemistry spectrum entry");
  require_true(
      std::string(spectrum->domain_object_key) == expected_key,
      std::string(context) + ": expected canonical chemistry domain_object_key");
  require_true(
      spectrum->subtype_revision == 1,
      std::string(context) + ": expected chemistry subtype revision 1");
  require_true(
      spectrum->source_format == expected_format,
      std::string(context) + ": expected chemistry source format");
  require_true(
      spectrum->coarse_kind == expected_coarse_kind,
      std::string(context) + ": expected chemistry coarse kind");
  require_true(
      spectrum->presence == expected_presence,
      std::string(context) + ": expected chemistry presence");
  require_true(
      spectrum->state == expected_state,
      std::string(context) + ": expected chemistry state");
  require_true(
      spectrum->flags == KERNEL_DOMAIN_OBJECT_FLAG_NONE,
      std::string(context) + ": expected chemistry flags cleared");
}

void test_chemistry_subtype_surface_lists_present_missing_unresolved_and_unsupported_states() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "spectra");
  std::filesystem::create_directories(vault / "assets");
  write_file_bytes(
      vault / "spectra" / "ready.jdx",
      make_jcamp_bytes("Ready", "NMR SPECTRUM", "PPM", "INTENSITY", 4));
  write_file_bytes(
      vault / "spectra" / "strict.csv",
      make_spectrum_csv("cm^-1", "absorbance", "ir_like", "Strict", "100,1\n200,2\n"));
  write_file_bytes(vault / "spectra" / "broken.csv", "x,y\n1,2\n");
  write_file_bytes(vault / "spectra" / "unsupported.sdf", "sdf-bytes");
  write_file_bytes(
      vault / "spectra" / "unreferenced.jdx",
      make_jcamp_bytes("Unref", "IR SPECTRUM", "CM^-1", "ABSORBANCE", 2));
  write_file_bytes(vault / "assets" / "plain.png", "png-bytes");

  kernel_handle* handle = nullptr;
  require_ok_status(kernel_open_vault(vault.string().c_str(), &handle), "chem subtype open vault");

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string note =
      "# Chemistry Subtype\n"
      "[Ready](spectra/ready.jdx)\n"
      "[Strict](spectra/strict.csv)\n"
      "[Broken](spectra/broken.csv)\n"
      "[Unsupported](spectra/unsupported.sdf)\n"
      "[Missing](spectra/missing.jdx)\n"
      "![Plain](assets/plain.png)\n";
  require_ok_status(
      kernel_write_note(
          handle,
          "chemistry-subtype.md",
          note.data(),
          note.size(),
          nullptr,
          &metadata,
          &disposition),
      "chem subtype write note");

  kernel_chem_spectrum_list spectra{};
  require_ok_status(
      kernel_query_chem_spectra(handle, static_cast<size_t>(-1), &spectra),
      "chem subtype query list");
  require_true(
      spectra.count == 5,
      "chem subtype list should expose only live chemistry candidates");
  require_true(
      std::string(spectra.spectra[0].attachment_rel_path) == "spectra/broken.csv" &&
          std::string(spectra.spectra[1].attachment_rel_path) == "spectra/missing.jdx" &&
          std::string(spectra.spectra[2].attachment_rel_path) == "spectra/ready.jdx" &&
          std::string(spectra.spectra[3].attachment_rel_path) == "spectra/strict.csv" &&
          std::string(spectra.spectra[4].attachment_rel_path) == "spectra/unsupported.sdf",
      "chem subtype list should sort by normalized attachment rel_path");

  require_spectrum_state(
      spectra,
      "spectra/broken.csv",
      KERNEL_CHEM_SPECTRUM_FORMAT_SPECTRUM_CSV_V1,
      KERNEL_ATTACHMENT_KIND_GENERIC_FILE,
      KERNEL_ATTACHMENT_PRESENCE_PRESENT,
      KERNEL_DOMAIN_OBJECT_UNRESOLVED,
      "dom:v1/attachment/spectra%2Fbroken.csv/chem/spectrum",
      "broken csv");
  require_spectrum_state(
      spectra,
      "spectra/missing.jdx",
      KERNEL_CHEM_SPECTRUM_FORMAT_JCAMP_DX,
      KERNEL_ATTACHMENT_KIND_CHEM_LIKE,
      KERNEL_ATTACHMENT_PRESENCE_MISSING,
      KERNEL_DOMAIN_OBJECT_MISSING,
      "dom:v1/attachment/spectra%2Fmissing.jdx/chem/spectrum",
      "missing jdx");
  require_spectrum_state(
      spectra,
      "spectra/ready.jdx",
      KERNEL_CHEM_SPECTRUM_FORMAT_JCAMP_DX,
      KERNEL_ATTACHMENT_KIND_CHEM_LIKE,
      KERNEL_ATTACHMENT_PRESENCE_PRESENT,
      KERNEL_DOMAIN_OBJECT_PRESENT,
      "dom:v1/attachment/spectra%2Fready.jdx/chem/spectrum",
      "ready jdx");
  require_spectrum_state(
      spectra,
      "spectra/strict.csv",
      KERNEL_CHEM_SPECTRUM_FORMAT_SPECTRUM_CSV_V1,
      KERNEL_ATTACHMENT_KIND_GENERIC_FILE,
      KERNEL_ATTACHMENT_PRESENCE_PRESENT,
      KERNEL_DOMAIN_OBJECT_PRESENT,
      "dom:v1/attachment/spectra%2Fstrict.csv/chem/spectrum",
      "strict csv");
  require_spectrum_state(
      spectra,
      "spectra/unsupported.sdf",
      KERNEL_CHEM_SPECTRUM_FORMAT_UNKNOWN,
      KERNEL_ATTACHMENT_KIND_CHEM_LIKE,
      KERNEL_ATTACHMENT_PRESENCE_PRESENT,
      KERNEL_DOMAIN_OBJECT_UNSUPPORTED,
      "dom:v1/attachment/spectra%2Funsupported.sdf/chem/spectrum",
      "unsupported sdf");
  kernel_free_chem_spectrum_list(&spectra);

  require_ok_status(
      kernel_query_chem_spectra(handle, 2, &spectra),
      "chem subtype query list with limit");
  require_true(
      spectra.count == 2 &&
          std::string(spectra.spectra[0].attachment_rel_path) == "spectra/broken.csv" &&
          std::string(spectra.spectra[1].attachment_rel_path) == "spectra/missing.jdx",
      "chem subtype list should apply limit after stable rel_path ordering");
  kernel_free_chem_spectrum_list(&spectra);

  size_t spectra_default_limit = 0;
  require_ok_status(
      kernel_get_chem_spectra_default_limit(&spectra_default_limit),
      "chem spectra default limit");
  require_true(
      spectra_default_limit == 512,
      "chem spectra default limit should be kernel-owned");
  require_true(
      kernel_get_chem_spectra_default_limit(nullptr).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "chem spectra default limit should require output pointer");

  kernel_chem_spectrum_record spectrum{};
  require_ok_status(
      kernel_get_chem_spectrum(handle, "spectra\\strict.csv", &spectrum),
      "chem subtype single lookup");
  require_true(
      std::string(spectrum.attachment_rel_path) == "spectra/strict.csv",
      "chem subtype single lookup should normalize backslash input");
  const std::string strict_key = spectrum.domain_object_key;
  kernel_free_chem_spectrum_record(&spectrum);

  require_ok_status(
      kernel_get_chem_spectrum(handle, "spectra/unsupported.sdf", &spectrum),
      "chem subtype unsupported single lookup");
  const std::string unsupported_key = spectrum.domain_object_key;
  kernel_free_chem_spectrum_record(&spectrum);

  require_true(
      kernel_get_chem_spectrum(handle, "assets/plain.png", &spectrum).code ==
          KERNEL_ERROR_NOT_FOUND,
      "chem subtype single lookup should reject non-chemistry carriers");
  require_true(
      spectrum.attachment_rel_path == nullptr && spectrum.domain_object_key == nullptr,
      "chem subtype single lookup should clear stale output on NOT_FOUND");
  require_true(
      kernel_get_chem_spectrum(handle, "spectra/unreferenced.jdx", &spectrum).code ==
          KERNEL_ERROR_NOT_FOUND,
      "chem subtype single lookup should reject unreferenced chemistry files");
  require_true(
      spectrum.attachment_rel_path == nullptr && spectrum.domain_object_key == nullptr,
      "chem subtype single lookup should clear stale output for non-live chemistry carriers");

  kernel_domain_object_descriptor object{};
  require_ok_status(
      kernel_get_domain_object(handle, strict_key.c_str(), &object),
      "chem subtype domain object roundtrip");
  require_true(
      std::string(object.domain_object_key) == strict_key &&
          std::string(object.carrier_key) == "spectra/strict.csv",
      "chem subtype domain object roundtrip should preserve canonical chemistry key");
  require_true(
      std::string(object.subtype_namespace) == "chem" &&
          std::string(object.subtype_name) == "spectrum",
      "chem subtype domain object roundtrip should preserve chem.spectrum subtype");
  require_true(
      object.state == KERNEL_DOMAIN_OBJECT_PRESENT,
      "chem subtype domain object roundtrip should preserve present state");
  kernel_free_domain_object_descriptor(&object);

  require_ok_status(
      kernel_get_domain_object(handle, unsupported_key.c_str(), &object),
      "chem subtype domain object unsupported roundtrip");
  require_true(
      object.state == KERNEL_DOMAIN_OBJECT_UNSUPPORTED,
      "chem subtype domain object roundtrip should preserve unsupported state");
  kernel_free_domain_object_descriptor(&object);

  require_true(
      kernel_get_domain_object(
          handle,
          "dom:v1/attachment/spectra/strict.csv/chem/spectrum",
          &object)
              .code == KERNEL_ERROR_INVALID_ARGUMENT,
      "chem subtype domain object lookup should reject unescaped chemistry carrier keys");

  require_ok_status(kernel_close(handle), "chem subtype close vault");
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_chemistry_subtype_surface_rejects_invalid_input_and_tracks_rebuild_updates() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "spectra");
  write_file_bytes(
      vault / "spectra" / "flip.jdx",
      make_jcamp_bytes("Flip", "NMR SPECTRUM", "PPM", "INTENSITY", 2));
  write_file_bytes(vault / "spectra" / "repair.csv", "x,y\n1,2\n");

  kernel_handle* handle = nullptr;
  require_ok_status(kernel_open_vault(vault.string().c_str(), &handle), "chem subtype rebuild open vault");

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string note =
      "# Chemistry Rebuild\n"
      "[Flip](spectra/flip.jdx)\n"
      "[Repair](spectra/repair.csv)\n";
  require_ok_status(
      kernel_write_note(
          handle,
          "chemistry-rebuild.md",
          note.data(),
          note.size(),
          nullptr,
          &metadata,
          &disposition),
      "chem subtype rebuild write note");

  kernel_chem_spectrum_record spectrum{};
  require_true(
      kernel_get_chem_spectrum(handle, "", &spectrum).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "chem subtype single lookup should reject empty attachment path");
  require_true(
      spectrum.attachment_rel_path == nullptr && spectrum.domain_object_key == nullptr,
      "chem subtype single lookup should clear stale output on invalid input");

  require_ok_status(
      kernel_get_chem_spectrum(handle, "spectra/flip.jdx", &spectrum),
      "chem subtype rebuild initial jdx lookup");
  require_true(
      spectrum.state == KERNEL_DOMAIN_OBJECT_PRESENT,
      "chem subtype rebuild test should start from present jdx state");
  kernel_free_chem_spectrum_record(&spectrum);

  require_ok_status(
      kernel_get_chem_spectrum(handle, "spectra/repair.csv", &spectrum),
      "chem subtype rebuild initial csv lookup");
  require_true(
      spectrum.state == KERNEL_DOMAIN_OBJECT_UNRESOLVED,
      "chem subtype rebuild test should start from unresolved csv state");
  kernel_free_chem_spectrum_record(&spectrum);

  write_file_bytes(
      vault / "spectra" / "flip.jdx",
      "##JCAMP-DX=5.01\n##TITLE=Broken\n##DATA TYPE=NMR SPECTRUM\n");
  write_file_bytes(
      vault / "spectra" / "repair.csv",
      make_spectrum_csv("ppm", "intensity", "nmr_like", "Repair", "1,1\n2,2\n3,3\n"));

  require_ok_status(kernel_rebuild_index(handle), "chem subtype rebuild run");

  require_ok_status(
      kernel_get_chem_spectrum(handle, "spectra/flip.jdx", &spectrum),
      "chem subtype rebuild updated jdx lookup");
  require_true(
      spectrum.state == KERNEL_DOMAIN_OBJECT_UNRESOLVED,
      "chem subtype rebuild should degrade invalidated jdx to unresolved");
  kernel_free_chem_spectrum_record(&spectrum);

  require_ok_status(
      kernel_get_chem_spectrum(handle, "spectra/repair.csv", &spectrum),
      "chem subtype rebuild updated csv lookup");
  require_true(
      spectrum.state == KERNEL_DOMAIN_OBJECT_PRESENT &&
          spectrum.source_format == KERNEL_CHEM_SPECTRUM_FORMAT_SPECTRUM_CSV_V1,
      "chem subtype rebuild should promote strict csv into present state");
  const std::string repair_key = spectrum.domain_object_key;
  kernel_free_chem_spectrum_record(&spectrum);

  require_ok_status(kernel_close(handle), "chem subtype rebuild close vault");

  handle = nullptr;
  require_ok_status(kernel_open_vault(vault.string().c_str(), &handle), "chem subtype reopen vault");
  kernel_domain_object_descriptor object{};
  require_ok_status(
      kernel_get_domain_object(handle, repair_key.c_str(), &object),
      "chem subtype reopen domain object lookup");
  require_true(
      object.state == KERNEL_DOMAIN_OBJECT_PRESENT &&
          std::string(object.carrier_key) == "spectra/repair.csv",
      "chem subtype reopen should preserve chemistry domain object identity");
  kernel_free_domain_object_descriptor(&object);

  require_ok_status(kernel_close(handle), "chem subtype reopen close vault");
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_chemistry_subtype_tests() {
  test_chemistry_subtype_surface_lists_present_missing_unresolved_and_unsupported_states();
  test_chemistry_subtype_surface_rejects_invalid_input_and_tracks_rebuild_updates();
}
