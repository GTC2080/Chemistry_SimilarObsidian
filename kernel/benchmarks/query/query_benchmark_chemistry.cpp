// Reason: This file keeps Track 5 chemistry-query benchmark loops and fixture
// seeding separate so the main query benchmark stays readable.

#include "benchmarks/query/query_benchmark_chemistry.h"

#include "benchmarks/benchmark_thresholds.h"
#include "chemistry/chemistry_spectrum_metadata.h"
#include "chemistry/chemistry_spectrum_selector.h"
#include "vault/revision.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

namespace {

bool expect_ok(const kernel_status status, const char* operation) {
  if (status.code == KERNEL_OK) {
    return true;
  }
  std::cerr << operation << " failed with code " << status.code << "\n";
  return false;
}

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

bool seed_note(
    kernel_handle* handle,
    const char* rel_path,
    const std::string& text) {
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  return expect_ok(
      kernel_write_note(
          handle,
          rel_path,
          text.data(),
          text.size(),
          nullptr,
          &metadata,
          &disposition),
      rel_path);
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

bool build_selector_for_file(
    const std::filesystem::path& vault_root,
    std::string_view rel_path,
    const bool whole_selector,
    std::string& out_selector) {
  out_selector.clear();
  const std::string bytes =
      read_file_bytes(vault_root / std::filesystem::path(rel_path));
  if (bytes.empty()) {
    std::cerr << "selector fixture file read failed for " << rel_path << "\n";
    return false;
  }

  const std::string attachment_content_revision =
      kernel::vault::compute_content_revision(bytes);
  const auto parsed = kernel::chemistry::extract_spectrum_metadata(
      rel_path,
      bytes,
      attachment_content_revision);
  if (parsed.status != kernel::chemistry::SpectrumParseStatus::Ready) {
    std::cerr << "selector fixture parse failed for " << rel_path << "\n";
    return false;
  }

  kernel::chemistry::ParsedChemSpectrumSelector selector;
  selector.kind = whole_selector
                      ? kernel::chemistry::ChemSpectrumSelectorKind::WholeSpectrum
                      : kernel::chemistry::ChemSpectrumSelectorKind::XRange;
  selector.chemistry_selector_basis_revision =
      kernel::chemistry::build_chemistry_selector_basis_revision(
          parsed.metadata.attachment_content_revision,
          kernel::chemistry::build_normalized_spectrum_basis(parsed.metadata));
  if (!whole_selector) {
    selector.start = "1";
    selector.end = "2";
    selector.unit = parsed.metadata.x_axis_unit;
  }
  out_selector = kernel::chemistry::serialize_chem_spectrum_selector(selector);
  return true;
}

}  // namespace

namespace kernel::benchmarks::query {

bool prepare_chemistry_query_benchmark_fixture(
    const std::filesystem::path& vault_root,
    kernel_handle* handle,
    ChemistryBenchmarkConfig& out_config) {
  out_config = ChemistryBenchmarkConfig{};

  constexpr std::string_view kCsvRelPath = "chem/bench-alpha.csv";
  constexpr std::string_view kSpectrumRelPath = "chem/bench-ready.jdx";
  constexpr std::string_view kLiveNoteRelPath = "chem/bench-live.md";
  constexpr std::string_view kSourceNoteRelPath = "chem/bench-source-refs.md";
  constexpr std::string_view kReferrerNoteRelPath = "chem/bench-referrer.md";

  if (!write_file_bytes(
          vault_root / std::filesystem::path(kCsvRelPath),
          make_spectrum_csv("ppm", "intensity", "nmr_like", "Bench CSV", "1,1\n2,2\n3,3\n")) ||
      !write_file_bytes(
          vault_root / std::filesystem::path(kSpectrumRelPath),
          make_jcamp_bytes("Bench Ready", "NMR SPECTRUM", "PPM", "INTENSITY", 4))) {
    return false;
  }

  if (!seed_note(
          handle,
          std::string(kLiveNoteRelPath).c_str(),
          "# Chemistry Bench Live\n"
          "[CSV](chem/bench-alpha.csv)\n"
          "[JDX](chem/bench-ready.jdx)\n")) {
    return false;
  }

  std::string whole_selector;
  std::string x_range_selector;
  if (!build_selector_for_file(vault_root, kSpectrumRelPath, true, whole_selector) ||
      !build_selector_for_file(vault_root, kSpectrumRelPath, false, x_range_selector)) {
    return false;
  }

  if (!seed_note(
          handle,
          std::string(kSourceNoteRelPath).c_str(),
          "# Chemistry Bench Source Refs\n"
          "[Whole](chem/bench-ready.jdx#chemsel=" + whole_selector + ")\n"
          "[Range](chem/bench-ready.jdx#chemsel=" + x_range_selector + ")\n") ||
      !seed_note(
          handle,
          std::string(kReferrerNoteRelPath).c_str(),
          "# Chemistry Bench Referrer\n"
          "[Whole](chem/bench-ready.jdx#chemsel=" + whole_selector + ")\n")) {
    return false;
  }

  out_config.metadata_rel_path = std::string(kSpectrumRelPath);
  out_config.catalog_first_rel_path = std::string(kCsvRelPath);
  out_config.lookup_rel_path = std::string(kSpectrumRelPath);
  out_config.note_source_rel_path = std::string(kSourceNoteRelPath);
  out_config.referrer_first_note_rel_path = std::string(kReferrerNoteRelPath);
  return true;
}

bool run_chemistry_query_benchmarks(
    kernel_handle* handle,
    const ChemistryBenchmarkConfig& config,
    const int iterations) {
  const auto metadata_start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i) {
    kernel_domain_metadata_list entries{};
    if (!expect_ok(
            kernel_query_chem_spectrum_metadata(
                handle,
                config.metadata_rel_path.c_str(),
                8,
                &entries),
            "chemistry metadata query")) {
      return false;
    }
    if (entries.count != 6 ||
        std::string(entries.entries[0].carrier_key) != config.metadata_rel_path ||
        std::string(entries.entries[0].namespace_name) != "chem.spectrum") {
      std::cerr << "chemistry metadata query returned unexpected state\n";
      return false;
    }
    kernel_free_domain_metadata_list(&entries);
  }
  const auto metadata_end = std::chrono::steady_clock::now();

  const auto catalog_start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i) {
    kernel_chem_spectrum_list spectra{};
    if (!expect_ok(
            kernel_query_chem_spectra(handle, 4, &spectra),
            "chemistry spectrum catalog query")) {
      return false;
    }
    if (spectra.count != 2 ||
        std::string(spectra.spectra[0].attachment_rel_path) !=
            config.catalog_first_rel_path ||
        spectra.spectra[0].state != KERNEL_DOMAIN_OBJECT_PRESENT) {
      std::cerr << "chemistry spectrum catalog query returned unexpected state\n";
      return false;
    }
    kernel_free_chem_spectrum_list(&spectra);
  }
  const auto catalog_end = std::chrono::steady_clock::now();

  const auto lookup_start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i) {
    kernel_chem_spectrum_record spectrum{};
    if (!expect_ok(
            kernel_get_chem_spectrum(
                handle,
                config.lookup_rel_path.c_str(),
                &spectrum),
            "chemistry spectrum lookup query")) {
      return false;
    }
    if (std::string(spectrum.attachment_rel_path) != config.lookup_rel_path ||
        spectrum.state != KERNEL_DOMAIN_OBJECT_PRESENT) {
      std::cerr << "chemistry spectrum lookup query returned unexpected state\n";
      return false;
    }
    kernel_free_chem_spectrum_record(&spectrum);
  }
  const auto lookup_end = std::chrono::steady_clock::now();

  const auto note_refs_start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i) {
    kernel_chem_spectrum_source_refs refs{};
    if (!expect_ok(
            kernel_query_note_chem_spectrum_refs(
                handle,
                config.note_source_rel_path.c_str(),
                4,
                &refs),
            "chemistry note source refs query")) {
      return false;
    }
    if (refs.count != 2 ||
        std::string(refs.refs[0].attachment_rel_path) != config.lookup_rel_path ||
        refs.refs[0].state != KERNEL_DOMAIN_REF_RESOLVED) {
      std::cerr << "chemistry note source refs query returned unexpected state\n";
      return false;
    }
    kernel_free_chem_spectrum_source_refs(&refs);
  }
  const auto note_refs_end = std::chrono::steady_clock::now();

  const auto referrers_start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i) {
    kernel_chem_spectrum_referrers referrers{};
    if (!expect_ok(
            kernel_query_chem_spectrum_referrers(
                handle,
                config.lookup_rel_path.c_str(),
                4,
                &referrers),
            "chemistry spectrum referrers query")) {
      return false;
    }
    if (referrers.count != 3 ||
        std::string(referrers.referrers[0].note_rel_path) !=
            config.referrer_first_note_rel_path ||
        referrers.referrers[0].state != KERNEL_DOMAIN_REF_RESOLVED) {
      std::cerr << "chemistry spectrum referrers query returned unexpected state\n";
      return false;
    }
    kernel_free_chem_spectrum_referrers(&referrers);
  }
  const auto referrers_end = std::chrono::steady_clock::now();

  const auto metadata_elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          metadata_end - metadata_start)
          .count();
  const auto catalog_elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          catalog_end - catalog_start)
          .count();
  const auto lookup_elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          lookup_end - lookup_start)
          .count();
  const auto note_refs_elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          note_refs_end - note_refs_start)
          .count();
  const auto referrers_elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          referrers_end - referrers_start)
          .count();

  const bool metadata_within_gate = report_gate(
      kChemistryMetadataQueryGate,
      metadata_elapsed_ms);
  const bool catalog_within_gate = report_gate(
      kChemistrySpectrumCatalogQueryGate,
      catalog_elapsed_ms);
  const bool lookup_within_gate = report_gate(
      kChemistrySpectrumLookupQueryGate,
      lookup_elapsed_ms);
  const bool note_refs_within_gate = report_gate(
      kChemistryNoteSpectrumRefsQueryGate,
      note_refs_elapsed_ms);
  const bool referrers_within_gate = report_gate(
      kChemistrySpectrumReferrersQueryGate,
      referrers_elapsed_ms);

  return metadata_within_gate &&
         catalog_within_gate &&
         lookup_within_gate &&
         note_refs_within_gate &&
         referrers_within_gate;
}

}  // namespace kernel::benchmarks::query
