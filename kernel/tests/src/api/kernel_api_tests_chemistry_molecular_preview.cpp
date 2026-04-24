// Reason: Keep molecular preview construction covered at the kernel ABI
// boundary so Tauri Rust stays a file IO bridge.

#include "kernel/c_api.h"

#include "api/kernel_api_chemistry_suites.h"
#include "support/test_support.h"

#include <string>
#include <string_view>

namespace {

void require_ok_status(const kernel_status status, std::string_view context) {
  require_true(
      status.code == KERNEL_OK,
      std::string(context) + ": expected KERNEL_OK, got status " +
          std::to_string(status.code));
}

void test_molecular_preview_truncates_pdb_atom_lines() {
  const std::string raw =
      "HEADER sample\n"
      "ATOM      1  N   GLY A   1       0.000   0.000   0.000\n"
      "HETATM    2  O   HOH A   2       1.000   0.000   0.000\n"
      "ATOM      3  C   GLY A   1       2.000   0.000   0.000\n"
      "END\n";
  kernel_molecular_preview preview{};

  require_ok_status(
      kernel_build_molecular_preview(raw.data(), raw.size(), "pdb", 2, &preview),
      "pdb molecular preview");

  require_true(preview.atom_count == 3, "pdb preview should count all atom lines");
  require_true(preview.preview_atom_count == 2, "pdb preview should keep requested atoms");
  require_true(preview.truncated == 1, "pdb preview should mark truncation");
  const std::string text = preview.preview_data == nullptr ? "" : preview.preview_data;
  require_true(text.find("HEADER sample") != std::string::npos, "pdb preview keeps header");
  require_true(text.find("HETATM") != std::string::npos, "pdb preview keeps hetatm");
  require_true(text.find("ATOM      3") == std::string::npos, "pdb preview drops extra atom");

  kernel_free_molecular_preview(&preview);
  require_true(
      preview.preview_data == nullptr && preview.atom_count == 0,
      "molecular preview free should reset output");
}

void test_molecular_preview_rewrites_xyz_header_count() {
  const std::string raw =
      "4\n"
      "water cluster\n"
      "O 0 0 0\n"
      "\n"
      "H 0 1 0\n"
      "H 0 -1 0\n";
  kernel_molecular_preview preview{};

  require_ok_status(
      kernel_build_molecular_preview(raw.data(), raw.size(), "xyz", 2, &preview),
      "xyz molecular preview");

  require_true(preview.atom_count == 3, "xyz preview should ignore blank atom rows");
  require_true(preview.preview_atom_count == 2, "xyz preview should clamp atom rows");
  require_true(preview.truncated == 1, "xyz preview should mark truncation");
  const std::string text = preview.preview_data == nullptr ? "" : preview.preview_data;
  require_true(text.rfind("2\nwater cluster\n", 0) == 0, "xyz preview rewrites count header");
  require_true(text.find("H 0 -1 0") == std::string::npos, "xyz preview drops extra atom");

  kernel_free_molecular_preview(&preview);
}

void test_molecular_preview_preserves_cif_text_and_rejects_unsupported() {
  const std::string raw = "data_test\n_cell_length_a 5\n";
  kernel_molecular_preview preview{};

  require_ok_status(
      kernel_build_molecular_preview(raw.data(), raw.size(), "cif", 2, &preview),
      "cif molecular preview");
  require_true(preview.atom_count == 0, "cif preview should not count atoms");
  require_true(
      preview.preview_data != nullptr && std::string(preview.preview_data) == raw,
      "cif preview should preserve raw text");
  kernel_free_molecular_preview(&preview);

  require_true(
      kernel_build_molecular_preview(raw.data(), raw.size(), "mol", 2, &preview).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "unsupported molecular preview extension should be rejected");
  require_true(
      preview.error == KERNEL_MOLECULAR_PREVIEW_ERROR_UNSUPPORTED_EXTENSION,
      "unsupported molecular preview should report typed error");
  require_true(
      kernel_build_molecular_preview(nullptr, 0, "pdb", 2, &preview).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "molecular preview should reject null raw");
  require_true(
      kernel_build_molecular_preview(raw.data(), raw.size(), nullptr, 2, &preview).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "molecular preview should reject null extension");
  require_true(
      kernel_build_molecular_preview(raw.data(), raw.size(), "pdb", 2, nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "molecular preview should reject null output");
}

}  // namespace

void run_chemistry_molecular_preview_tests() {
  test_molecular_preview_truncates_pdb_atom_lines();
  test_molecular_preview_rewrites_xyz_header_count();
  test_molecular_preview_preserves_cif_text_and_rejects_unsupported();
}
