// Reason: This file exposes symmetry molecule parsing through the kernel C ABI
// so Tauri Rust no longer owns PDB/XYZ/CIF atom parsing rules.

#include "kernel/c_api.h"

#include "core/kernel_shared.h"
#include "symmetry/atom_parser.h"

#include <new>
#include <string_view>

namespace {

void reset_symmetry_atom_list_impl(kernel_symmetry_atom_list* atoms) {
  if (atoms == nullptr) {
    return;
  }
  if (atoms->atoms != nullptr) {
    for (std::size_t index = 0; index < atoms->count; ++index) {
      delete[] atoms->atoms[index].element;
      atoms->atoms[index].element = nullptr;
    }
    delete[] atoms->atoms;
  }
  atoms->atoms = nullptr;
  atoms->count = 0;
  atoms->error = KERNEL_SYMMETRY_PARSE_ERROR_NONE;
}

bool fill_symmetry_atom_list(
    const kernel::symmetry::SymmetryAtomParseResult& source,
    kernel_symmetry_atom_list* out_atoms) {
  if (source.atoms.empty()) {
    return true;
  }

  out_atoms->atoms = new (std::nothrow) kernel_symmetry_atom_record[source.atoms.size()]{};
  if (out_atoms->atoms == nullptr) {
    return false;
  }
  out_atoms->count = source.atoms.size();

  for (std::size_t index = 0; index < source.atoms.size(); ++index) {
    const auto& source_atom = source.atoms[index];
    auto& target = out_atoms->atoms[index];
    target.element = kernel::core::duplicate_c_string(source_atom.element);
    if (target.element == nullptr) {
      return false;
    }
    target.position[0] = source_atom.position[0];
    target.position[1] = source_atom.position[1];
    target.position[2] = source_atom.position[2];
    target.mass = source_atom.mass;
  }

  return true;
}

}  // namespace

extern "C" kernel_status kernel_parse_symmetry_atoms_text(
    const char* raw,
    const std::size_t raw_size,
    const char* format,
    kernel_symmetry_atom_list* out_atoms) {
  if (out_atoms == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  reset_symmetry_atom_list_impl(out_atoms);
  if (raw == nullptr || format == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const auto parsed = kernel::symmetry::parse_symmetry_atoms_text(
      std::string_view(raw, raw_size),
      std::string_view(format));
  if (parsed.error != KERNEL_SYMMETRY_PARSE_ERROR_NONE) {
    out_atoms->error = parsed.error;
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  if (!fill_symmetry_atom_list(parsed, out_atoms)) {
    reset_symmetry_atom_list_impl(out_atoms);
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }

  return kernel::core::make_status(KERNEL_OK);
}

extern "C" void kernel_free_symmetry_atom_list(kernel_symmetry_atom_list* atoms) {
  reset_symmetry_atom_list_impl(atoms);
}
