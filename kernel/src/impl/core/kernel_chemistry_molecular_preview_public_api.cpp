// Reason: This file exposes molecular preview construction through the kernel
// C ABI so Tauri Rust no longer owns PDB/XYZ preview rules.

#include "kernel/c_api.h"

#include "chemistry/molecular_preview.h"
#include "core/kernel_shared.h"

#include <algorithm>
#include <new>
#include <string_view>

namespace {

constexpr std::size_t kDefaultPreviewAtomLimit = 2000;
constexpr std::size_t kMinPreviewAtomLimit = 200;
constexpr std::size_t kMaxPreviewAtomLimit = 20000;

void reset_molecular_preview_impl(kernel_molecular_preview* preview) {
  if (preview == nullptr) {
    return;
  }
  delete[] preview->preview_data;
  preview->preview_data = nullptr;
  preview->atom_count = 0;
  preview->preview_atom_count = 0;
  preview->truncated = 0;
  preview->error = KERNEL_MOLECULAR_PREVIEW_ERROR_NONE;
}

bool fill_molecular_preview(
    const kernel::chemistry::MolecularPreviewComputation& source,
    kernel_molecular_preview* out_preview) {
  out_preview->preview_data = kernel::core::duplicate_c_string(source.preview_data);
  if (out_preview->preview_data == nullptr) {
    return false;
  }
  out_preview->atom_count = source.atom_count;
  out_preview->preview_atom_count = source.preview_atom_count;
  out_preview->truncated = source.truncated ? 1 : 0;
  out_preview->error = KERNEL_MOLECULAR_PREVIEW_ERROR_NONE;
  return true;
}

}  // namespace

extern "C" kernel_status kernel_normalize_molecular_preview_atom_limit(
    const std::size_t requested_atoms,
    std::size_t* out_atoms) {
  if (out_atoms == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::size_t candidate =
      requested_atoms == 0 ? kDefaultPreviewAtomLimit : requested_atoms;
  *out_atoms = std::clamp(candidate, kMinPreviewAtomLimit, kMaxPreviewAtomLimit);
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_build_molecular_preview(
    const char* raw,
    const size_t raw_size,
    const char* extension,
    const size_t max_atoms,
    kernel_molecular_preview* out_preview) {
  reset_molecular_preview_impl(out_preview);
  if (raw == nullptr || extension == nullptr || out_preview == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const auto preview = kernel::chemistry::build_molecular_preview(
      std::string_view(raw, raw_size),
      std::string_view(extension),
      max_atoms);
  if (preview.error != KERNEL_MOLECULAR_PREVIEW_ERROR_NONE) {
    out_preview->error = preview.error;
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  if (!fill_molecular_preview(preview, out_preview)) {
    reset_molecular_preview_impl(out_preview);
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }

  return kernel::core::make_status(KERNEL_OK);
}

extern "C" void kernel_free_molecular_preview(kernel_molecular_preview* preview) {
  reset_molecular_preview_impl(preview);
}
