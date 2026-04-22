// Reason: Keep Track 5 Batch 1 chemistry metadata coverage separate so the
// first chemistry capability surface can evolve without bloating generic
// domain or attachment suites.

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

const kernel_domain_metadata_entry* find_entry(
    const kernel_domain_metadata_list& entries,
    const char* key_name) {
  for (size_t index = 0; index < entries.count; ++index) {
    if (entries.entries[index].key_name != nullptr &&
        std::string(entries.entries[index].key_name) == key_name) {
      return &entries.entries[index];
    }
  }
  return nullptr;
}

void require_ok_status(const kernel_status status, std::string_view context) {
  require_true(
      status.code == KERNEL_OK,
      std::string(context) + ": expected KERNEL_OK, got status " +
          std::to_string(status.code));
}

void require_token_entry(
    const kernel_domain_metadata_list& entries,
    const char* key_name,
    std::string_view expected_value,
    std::string_view context) {
  const auto* entry = find_entry(entries, key_name);
  require_true(entry != nullptr, std::string(context) + ": expected token entry");
  require_true(
      entry->carrier_kind == KERNEL_DOMAIN_CARRIER_ATTACHMENT,
      std::string(context) + ": expected attachment carrier kind");
  require_true(
      entry->namespace_name != nullptr && std::string(entry->namespace_name) == "chem.spectrum",
      std::string(context) + ": expected chem.spectrum namespace");
  require_true(
      entry->public_schema_revision == 1,
      std::string(context) + ": expected chemistry namespace revision 1");
  require_true(
      entry->value_kind == KERNEL_DOMAIN_VALUE_TOKEN &&
          entry->string_value != nullptr &&
          std::string(entry->string_value) == expected_value,
      std::string(context) + ": expected token value");
}

void require_string_entry(
    const kernel_domain_metadata_list& entries,
    const char* key_name,
    std::string_view expected_value,
    std::string_view context) {
  const auto* entry = find_entry(entries, key_name);
  require_true(entry != nullptr, std::string(context) + ": expected string entry");
  require_true(
      entry->value_kind == KERNEL_DOMAIN_VALUE_STRING &&
          entry->string_value != nullptr &&
          std::string(entry->string_value) == expected_value,
      std::string(context) + ": expected string value");
}

void require_uint_entry(
    const kernel_domain_metadata_list& entries,
    const char* key_name,
    const std::uint64_t expected_value,
    std::string_view context) {
  const auto* entry = find_entry(entries, key_name);
  require_true(entry != nullptr, std::string(context) + ": expected uint entry");
  require_true(
      entry->value_kind == KERNEL_DOMAIN_VALUE_UINT64 &&
          entry->uint64_value == expected_value,
      std::string(context) + ": expected uint value");
}

void require_not_found(
    kernel_handle* handle,
    const char* attachment_rel_path,
    std::string_view context) {
  kernel_domain_metadata_list entries{};
  const kernel_status status =
      kernel_query_chem_spectrum_metadata(handle, attachment_rel_path, static_cast<size_t>(-1), &entries);
  require_true(
      status.code == KERNEL_ERROR_NOT_FOUND,
      std::string(context) + ": chemistry metadata query should return NOT_FOUND");
  require_true(
      entries.entries == nullptr && entries.count == 0,
      std::string(context) + ": chemistry metadata query should clear stale output on NOT_FOUND");
}

void test_chemistry_metadata_surface_projects_jcamp_and_strict_csv_entries() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "spectra");
  write_file_bytes(
      vault / "spectra" / "sample-ir.jdx",
      make_jcamp_bytes("  IR Sample  ", "INFRARED SPECTRUM", "CM^-1", "ABSORBANCE", 3));
  write_file_bytes(
      vault / "spectra" / "sample-nmr.csv",
      make_spectrum_csv("ppm", "relative intensity", "nmr_like", "\t CSV Sample  ", "1.0,5\n2.0,6\n3.0,7\n"));

  kernel_handle* handle = nullptr;
  require_ok_status(kernel_open_vault(vault.string().c_str(), &handle), "chemistry metadata open vault");

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string note =
      "# Chemistry Metadata\n"
      "[IR](spectra/sample-ir.jdx)\n"
      "[NMR](spectra/sample-nmr.csv)\n";
  require_ok_status(
      kernel_write_note(
          handle,
          "chemistry-metadata.md",
          note.data(),
          note.size(),
          nullptr,
          &metadata,
          &disposition),
      "chemistry metadata write note");

  kernel_domain_metadata_list entries{};
  require_ok_status(
      kernel_query_chem_spectrum_metadata(
          handle,
          "spectra/sample-ir.jdx",
          static_cast<size_t>(-1),
          &entries),
      "chemistry metadata query jcamp");
  require_true(entries.count == 6, "jcamp metadata should export six chemistry entries");
  require_true(
      std::string(entries.entries[0].key_name) == "family" &&
          std::string(entries.entries[1].key_name) == "point_count" &&
          std::string(entries.entries[2].key_name) == "sample_label" &&
          std::string(entries.entries[3].key_name) == "source_format" &&
          std::string(entries.entries[4].key_name) == "x_axis_unit" &&
          std::string(entries.entries[5].key_name) == "y_axis_unit",
      "jcamp metadata should sort chemistry keys by key_name");
  require_true(
      std::string(entries.entries[0].carrier_key) == "spectra/sample-ir.jdx",
      "jcamp metadata should preserve normalized live attachment rel_path");
  require_token_entry(entries, "family", "ir_like", "jcamp family");
  require_uint_entry(entries, "point_count", 3, "jcamp point count");
  require_string_entry(entries, "sample_label", "IR Sample", "jcamp sample label");
  require_token_entry(entries, "source_format", "jcamp_dx", "jcamp source format");
  require_string_entry(entries, "x_axis_unit", "cm-1", "jcamp x unit");
  require_string_entry(entries, "y_axis_unit", "absorbance", "jcamp y unit");
  kernel_free_domain_metadata_list(&entries);

  require_ok_status(
      kernel_query_chem_spectrum_metadata(handle, "spectra\\sample-nmr.csv", 3, &entries),
      "chemistry metadata query csv with limit");
  require_true(entries.count == 3, "chemistry metadata limit should trim the sorted entry list");
  require_true(
      std::string(entries.entries[0].carrier_key) == "spectra/sample-nmr.csv",
      "chemistry metadata should normalize backslash input");
  kernel_free_domain_metadata_list(&entries);

  require_ok_status(
      kernel_query_chem_spectrum_metadata(
          handle,
          "spectra/sample-nmr.csv",
          static_cast<size_t>(-1),
          &entries),
      "chemistry metadata query csv");
  require_true(entries.count == 6, "strict spectrum csv should export six chemistry entries");
  require_token_entry(entries, "family", "nmr_like", "csv family");
  require_uint_entry(entries, "point_count", 3, "csv point count");
  require_string_entry(entries, "sample_label", "CSV Sample", "csv sample label");
  require_token_entry(entries, "source_format", "spectrum_csv_v1", "csv source format");
  require_string_entry(entries, "x_axis_unit", "ppm", "csv x unit");
  require_string_entry(entries, "y_axis_unit", "relative-intensity", "csv y unit");
  kernel_free_domain_metadata_list(&entries);

  require_ok_status(kernel_close(handle), "chemistry metadata close vault");
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_chemistry_metadata_surface_rejects_invalid_unresolved_missing_and_non_live_inputs() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "spectra");
  write_file_bytes(vault / "spectra" / "broken.csv", "x,y\n1,2\n");
  write_file_bytes(
      vault / "spectra" / "oversize.csv",
      make_spectrum_csv(
          "nm",
          "intensity",
          "uv_like",
          std::string(160, 'a'),
          "200,1\n250,2\n300,3\n"));
  write_file_bytes(vault / "spectra" / "plain.txt", "plain-text");
  write_file_bytes(
      vault / "spectra" / "rebuild.jdx",
      make_jcamp_bytes("Before", "MASS SPECTRUM", "M/Z", "INTENSITY", 2));
  write_file_bytes(vault / "spectra" / "unreferenced.jdx", make_jcamp_bytes("Unref", "NMR", "PPM", "INTENSITY", 2));

  kernel_handle* handle = nullptr;
  require_ok_status(kernel_open_vault(vault.string().c_str(), &handle), "chemistry validation open vault");

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string note =
      "# Chemistry Validation\n"
      "[Broken](spectra/broken.csv)\n"
      "[Oversize](spectra/oversize.csv)\n"
      "[Missing](spectra/missing.jdx)\n"
      "[Rebuild](spectra/rebuild.jdx)\n"
      "[Plain](spectra/plain.txt)\n";
  require_ok_status(
      kernel_write_note(
          handle,
          "chemistry-validation.md",
          note.data(),
          note.size(),
          nullptr,
          &metadata,
          &disposition),
      "chemistry validation write note");

  kernel_domain_metadata_list entries{};
  require_true(
      kernel_query_chem_spectrum_metadata(handle, "", static_cast<size_t>(-1), &entries).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "chemistry metadata should reject empty attachment path");
  require_true(
      entries.entries == nullptr && entries.count == 0,
      "chemistry metadata should clear stale output on invalid input");

  require_not_found(handle, "spectra/broken.csv", "non-conforming strict csv");
  require_not_found(handle, "spectra/missing.jdx", "missing live spectrum");
  require_not_found(handle, "spectra/plain.txt", "non-spectrum live attachment");
  require_not_found(handle, "spectra/unreferenced.jdx", "unreferenced disk spectrum");

  require_ok_status(
      kernel_query_chem_spectrum_metadata(
          handle,
          "spectra/oversize.csv",
          static_cast<size_t>(-1),
          &entries),
      "chemistry validation query oversize csv");
  require_true(
      entries.count == 5,
      "oversize sample_label should be excluded without dropping the stable chemistry metadata row");
  require_true(
      find_entry(entries, "sample_label") == nullptr,
      "oversize sample_label should not enter the public chemistry surface");
  require_token_entry(entries, "family", "uv_like", "oversize csv family");
  kernel_free_domain_metadata_list(&entries);

  write_file_bytes(
      vault / "spectra" / "rebuild.jdx",
      make_jcamp_bytes("After", "NMR SPECTRUM", "PPM", "INTENSITY", 5));
  require_ok_status(kernel_rebuild_index(handle), "chemistry validation rebuild");
  require_ok_status(
      kernel_query_chem_spectrum_metadata(
          handle,
          "spectra/rebuild.jdx",
          static_cast<size_t>(-1),
          &entries),
      "chemistry validation query rebuilt jcamp");
  require_token_entry(entries, "family", "nmr_like", "rebuild chemistry family");
  require_uint_entry(entries, "point_count", 5, "rebuild chemistry point count");
  require_string_entry(entries, "sample_label", "After", "rebuild chemistry sample label");
  require_string_entry(entries, "x_axis_unit", "ppm", "rebuild chemistry x unit");
  kernel_free_domain_metadata_list(&entries);

  require_ok_status(kernel_close(handle), "chemistry validation close vault");
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_chemistry_metadata_tests() {
  test_chemistry_metadata_surface_projects_jcamp_and_strict_csv_entries();
  test_chemistry_metadata_surface_rejects_invalid_unresolved_missing_and_non_live_inputs();
}
