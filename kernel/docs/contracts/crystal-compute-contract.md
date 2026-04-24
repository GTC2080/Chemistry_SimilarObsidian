<!-- Reason: This file freezes the first stateless crystal compute ABI so crystal migration can proceed without mixing viewer DTOs, CIF parsing, and numeric kernel rules. -->

# Crystal Compute Contract

Last updated: `2026-04-24`

## Scope

This document covers stateless crystal computation surfaces.

Current surface:

- `kernel_parse_cif_crystal(raw, raw_size, out_result)`
- `kernel_free_crystal_parse_result(out_result)`
- `kernel_calculate_miller_plane(cell, h, k, l, out_result)`
- `kernel_build_supercell(cell, atoms, atom_count, symops, symop_count, nx, ny, nz, out_result)`
- `kernel_free_supercell_result(out_result)`

Current exclusions:

- viewer state
- vault indexing or attachment truth

## Boundary

Frozen rules:

- the surface is handle-free and must not read or write vault state
- Tauri Rust owns serde command marshalling and localized error text
- the kernel owns CIF cell, fractional atom, and symmetry operation parsing
- the kernel owns Miller-plane numeric geometry
- the kernel owns symmetry expansion, fractional-coordinate deduplication,
  supercell expansion, and Cartesian coordinate generation
- parsed CIF atom arrays, element strings, and symmetry operation arrays are
  kernel-owned until released with `kernel_free_crystal_parse_result(...)`
- Miller-plane output buffers are host-owned and require no kernel free call
- supercell atom arrays and element strings are kernel-owned until released with
  `kernel_free_supercell_result(...)`
- failures return `KERNEL_ERROR_INVALID_ARGUMENT` with a typed
  crystal compute error

## Miller Plane Rules

Frozen rules:

- `(h, k, l)` must not all be zero
- cell vectors derive from `a`, `b`, `c`, `alpha`, `beta`, and `gamma`
- `gamma` with near-zero sine is invalid
- invalid cell geometry that cannot construct a real basis is invalid
- reciprocal vectors use the cell volume determinant
- near-zero volume is invalid
- the Miller normal is `h*a* + k*b* + l*c*`
- the returned normal is normalized
- `d` is the plane equation offset `-d_spacing`
- `center` lies on the normal at `d_spacing`
- `vertices` are four visualization vertices centered on the plane

## Host Contract

Frozen rules:

- hosts keep user-facing localized error text
- hosts must not reimplement CIF cell, fractional atom, or symmetry operation parsing
- hosts must not reimplement Miller-plane geometry
- hosts must not reimplement supercell expansion or atom deduplication
- hosts must preserve the existing Tauri command shape for
  `calculate_miller_plane` and `parse_and_build_lattice`
- frontends continue to consume host commands rather than kernel ABI directly

## Supercell Rules

Frozen rules:

- atom inputs use parsed element labels and fractional coordinates
- symmetry inputs use a 3x3 rotation matrix and 3-vector translation
- generated fractional coordinates are normalized with Euclidean remainder into
  `[0, 1)`
- deduplication uses the existing `1 / 0.02` fractional grid tolerance
- deduplication key includes the element label and quantized fractional
  coordinate
- expanded atoms are emitted in `ix`, `iy`, `iz`, unit-atom order
- Cartesian coordinates are calculated from the crystal cell basis
- total emitted atoms must not exceed `50000`
- too-large expansions report the estimated atom count
