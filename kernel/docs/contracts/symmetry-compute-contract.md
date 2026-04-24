<!-- Reason: This file freezes stateless symmetry compute ABI slices so point-group and molecule parsing rules can move out of the Tauri Rust backend without mixing viewer DTOs into the kernel boundary. -->

# Symmetry Compute Contract

## Scope

This document covers stateless molecular symmetry computation surfaces.

Current surface:

- `kernel_parse_symmetry_atoms_text(raw, raw_size, format, out_atoms)`
- `kernel_free_symmetry_atom_list(out_atoms)`
- `kernel_classify_point_group(axes, axis_count, planes, plane_count, has_inversion, out_result)`

## Ownership

- the host owns raw molecule text and format strings
- the kernel owns `XYZ`, `PDB`, and simple `CIF` atom parsing rules
- returned symmetry atom arrays and element strings are kernel-owned until
  released with `kernel_free_symmetry_atom_list(...)`
- the host owns input axis and plane arrays
- the kernel owns point-group classification rules
- `kernel_symmetry_classification_result` is filled by value and requires no free call
- invalid pointer/count combinations return `KERNEL_ERROR_INVALID_ARGUMENT`

## Host Boundary

The Tauri host keeps:

- candidate axis and mirror-plane search
- render geometry DTO construction
- localized UI error wording

The kernel owns:

- molecule text parsing for `XYZ`, `PDB`, and simple `CIF`
- element normalization and atomic mass lookup for parsed symmetry atoms
- fractional-to-cartesian conversion for simple CIF atom loops
- point-group decision ordering
- low-symmetry classification (`C_1`, `C_s`, `C_i`)
- cyclic, dihedral, tetrahedral, octahedral, and icosahedral point-group labels

## Frozen Rules

- supported parser formats are `xyz`, `pdb`, and `cif`
- `XYZ` parsing follows the existing two-line header rule and skips short atom
  rows
- `PDB` parsing reads `ATOM` / `HETATM` fixed-width coordinates and prefers the
  element column when present
- `CIF` parsing supports simple `_atom_site_*` loops with either Cartesian or
  fractional coordinates
- simple CIF fractional coordinates require complete `_cell_length_*` and
  `_cell_angle_*` parameters
- parser failures report typed `kernel_symmetry_parse_error` values so hosts
  keep localized command messages without owning parsing rules
- axes must be sorted with the principal/highest-order axis first
- axis directions and plane normals are expected to be normalized by the host
- `has_inversion` is passed as a boolean byte
- returned labels are null-terminated UTF-8 stored in a fixed host-owned buffer
- the current ABI intentionally does not emit candidate axes, mirror planes, or
  render geometry
