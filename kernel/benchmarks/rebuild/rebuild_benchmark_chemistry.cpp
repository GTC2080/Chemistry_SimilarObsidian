// Reason: This file keeps Track 5 chemistry rebuild-fixture seeding separate
// so the rebuild benchmark does not bloat as chemistry coverage lands.

#include "benchmarks/rebuild/rebuild_benchmark_chemistry.h"

#include "chemistry/chemistry_spectrum_metadata.h"
#include "chemistry/chemistry_spectrum_selector.h"
#include "vault/revision.h"

#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

namespace {

bool write_file_bytes(const std::filesystem::path& path, std::string_view bytes) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream) {
    std::cerr << "failed to create file: " << path << "\n";
    return false;
  }
  stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  return stream.good();
}

std::string read_file_bytes(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    return {};
  }
  return std::string(
      std::istreambuf_iterator<char>(stream),
      std::istreambuf_iterator<char>());
}

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

bool build_whole_selector_for_file(
    const std::filesystem::path& vault_root,
    std::string_view rel_path,
    std::string& out_selector) {
  out_selector.clear();
  const std::string bytes =
      read_file_bytes(vault_root / std::filesystem::path(rel_path));
  if (bytes.empty()) {
    return false;
  }

  const std::string attachment_content_revision =
      kernel::vault::compute_content_revision(bytes);
  const auto parsed = kernel::chemistry::extract_spectrum_metadata(
      rel_path,
      bytes,
      attachment_content_revision);
  if (parsed.status != kernel::chemistry::SpectrumParseStatus::Ready) {
    return false;
  }

  kernel::chemistry::ParsedChemSpectrumSelector selector;
  selector.kind = kernel::chemistry::ChemSpectrumSelectorKind::WholeSpectrum;
  selector.chemistry_selector_basis_revision =
      kernel::chemistry::build_chemistry_selector_basis_revision(
          parsed.metadata.attachment_content_revision,
          kernel::chemistry::build_normalized_spectrum_basis(parsed.metadata));
  out_selector = kernel::chemistry::serialize_chem_spectrum_selector(selector);
  return true;
}

}  // namespace

namespace kernel::benchmarks::rebuild {

bool seed_chemistry_rebuild_fixture(
    const std::filesystem::path& vault_root,
    const int chemistry_spectrum_count) {
  std::filesystem::create_directories(vault_root / "chem");

  for (int i = 0; i < chemistry_spectrum_count; ++i) {
    const bool jcamp = (i % 2) == 0;
    const std::string suffix = std::to_string(i);
    const std::string rel_path =
        jcamp ? "chem/spectrum-" + suffix + ".jdx"
              : "chem/spectrum-" + suffix + ".csv";
    const std::filesystem::path absolute_path =
        vault_root / std::filesystem::path(rel_path);
    const bool seeded = jcamp
                            ? write_file_bytes(
                                  absolute_path,
                                  make_jcamp_bytes(
                                      "Bench " + suffix,
                                      "NMR SPECTRUM",
                                      "PPM",
                                      "INTENSITY",
                                      4 + (i % 3)))
                            : write_file_bytes(
                                  absolute_path,
                                  make_spectrum_csv(
                                      "ppm",
                                      "intensity",
                                      "nmr_like",
                                      "Bench " + suffix,
                                      "1,1\n2,2\n3,3\n"));
    if (!seeded) {
      return false;
    }

    std::string selector;
    if (!build_whole_selector_for_file(vault_root, rel_path, selector)) {
      std::cerr << "failed to build chemistry rebuild selector for " << rel_path
                << "\n";
      return false;
    }

    const std::filesystem::path note_path =
        vault_root / ("chem-note-" + suffix + ".md");
    std::ofstream note_output(note_path, std::ios::binary | std::ios::trunc);
    if (!note_output) {
      std::cerr << "seed write failed for chemistry note " << suffix << "\n";
      return false;
    }
    note_output << "# Chemistry Note " << suffix << "\n";
    note_output << "[Spectrum](" << rel_path << ")\n";
    note_output << "[Source](" << rel_path << "#chemsel=" << selector << ")\n";
    note_output << "#chembench\n";
  }

  return true;
}

}  // namespace kernel::benchmarks::rebuild
