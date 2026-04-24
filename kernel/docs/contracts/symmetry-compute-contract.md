<!-- Reason: This file freezes stateless symmetry compute ABI slices so point-group and molecule parsing rules can move out of the Tauri Rust backend without mixing viewer DTOs into the kernel boundary. -->

# Symmetry Compute Contract

## Scope

This document covers stateless molecular symmetry computation surfaces.

Current surface:

- `kernel_calculate_symmetry(raw, raw_size, format, max_atoms, out_result)`
- `kernel_free_symmetry_calculation_result(out_result)`
- `kernel_parse_symmetry_atoms_text(raw, raw_size, format, out_atoms)`
- `kernel_free_symmetry_atom_list(out_atoms)`
- `kernel_classify_point_group(axes, axis_count, planes, plane_count, has_inversion, out_result)`
- `kernel_analyze_symmetry_shape(atoms, atom_count, out_result)`
- `kernel_compute_symmetry_principal_axes(atoms, atom_count, out_axes)`
- `kernel_generate_symmetry_candidate_directions(atoms, atom_count, principal_axes, principal_axis_count, out_directions, out_direction_capacity, out_direction_count)`
- `kernel_generate_symmetry_candidate_planes(atoms, atom_count, found_axes, axis_count, principal_axes, principal_axis_count, out_planes, out_plane_capacity, out_plane_count)`
- `kernel_find_symmetry_rotation_axes(atoms, atom_count, candidates, candidate_count, out_axes, out_axis_capacity, out_axis_count)`
- `kernel_find_symmetry_mirror_planes(atoms, atom_count, candidates, candidate_count, out_planes, out_plane_capacity, out_plane_count)`
- `kernel_build_symmetry_render_geometry(axes, axis_count, planes, plane_count, mol_radius, out_axes, out_planes)`

## Ownership

- the host owns raw molecule text and format strings
- the kernel owns the end-to-end `calculate_symmetry` orchestration pipeline
- returned full symmetry calculation axes and planes are kernel-owned until
  released with `kernel_free_symmetry_calculation_result(...)`
- the kernel owns `XYZ`, `PDB`, and simple `CIF` atom parsing rules
- returned symmetry atom arrays and element strings are kernel-owned until
  released with `kernel_free_symmetry_atom_list(...)`
- the host owns input axis and plane arrays
- the host owns shape-analysis atom input arrays and element strings
- the kernel owns point-group classification rules
- the kernel owns center-of-mass, radius, linearity, linear-axis, and inversion
  matching rules for shape analysis
- the host owns the preallocated 3-slot principal-axis output array
- the kernel owns mass-weighted inertia tensor and principal-axis eigenvector
  calculation
- the host owns preallocated candidate output arrays and count pointers
- the kernel owns candidate direction and mirror-plane normal generation
- the host owns preallocated operation-search output arrays and count pointers
- the kernel owns candidate rotation-axis and mirror-plane validation
- `kernel_symmetry_classification_result` is filled by value and requires no free call
- `kernel_symmetry_shape_result` is filled by value and requires no free call
- the host owns preallocated render axis and plane output arrays
- the kernel owns axis endpoint and mirror-plane vertex construction
- invalid pointer/count combinations return `KERNEL_ERROR_INVALID_ARGUMENT`

## Host Boundary

The Tauri host keeps:

- command DTO construction
- full-result ABI marshalling
- localized UI error wording

The kernel owns:

- `calculate_symmetry` pipeline ordering from parse through render geometry
- molecule text parsing for `XYZ`, `PDB`, and simple `CIF`
- element normalization and atomic mass lookup for parsed symmetry atoms
- fractional-to-cartesian conversion for simple CIF atom loops
- mass-weighted center-of-mass calculation
- viewer radius calculation
- linear molecule detection and linear-axis selection
- inversion-center operation matching
- mass-weighted principal-axis calculation for non-linear molecules
- candidate rotation-axis direction generation
- candidate mirror-plane normal generation
- rotation operation matching for candidate axes
- reflection operation matching for candidate mirror planes
- point-group decision ordering
- low-symmetry classification (`C_1`, `C_s`, `C_i`)
- cyclic, dihedral, tetrahedral, octahedral, and icosahedral point-group labels
- rotation-axis render endpoint construction
- mirror-plane render vertex construction
- render-ready full symmetry result ownership and release

## Frozen Rules

- full calculation accepts the same raw molecule text and format strings as
  the parser surface
- full calculation enforces the host-provided atom-count limit and returns
  `KERNEL_SYMMETRY_CALC_ERROR_TOO_MANY_ATOMS` with the parsed atom count when
  exceeded
- full calculation maps parser failures through
  `KERNEL_SYMMETRY_CALC_ERROR_PARSE` and preserves the typed parser error
- full calculation returns `K_h` for a single atom without emitting axes or
  planes
- full calculation returns `D∞h` or `C∞v` for linear molecules and emits the
  linear axis with `order = 0`
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
- shape analysis requires at least one atom and rejects null element pointers
- shape analysis falls back to arithmetic center when total mass is near zero
- shape analysis keeps the viewer radius minimum at `1.0`
- principal-axis calculation expects centered atom positions from the host
- principal-axis calculation uses the provided atom masses and returns three
  normalized directions sorted by ascending inertia eigenvalue
- candidate generation expects centered atom positions when called directly
- candidate generation expects principal axes produced by the kernel when
  called through the full calculation surface
- candidate direction generation uses principal axes, Cartesian unit axes, atom
  vectors, same-element pair midpoints/differences, and up to 20 atom-direction
  cross products
- candidate plane generation uses found axes, principal axes, Cartesian unit
  axes, atom vectors, same-element pair differences/midpoints, and
  axis-by-atom cross products
- operation search expects host-provided candidate directions/normals when
  called directly
- operation search returns found axes sorted by descending order
- operation search checks rotation orders `6, 5, 4, 3, 2`
- operation search uses the frozen atom matching tolerance of `0.30`
- operation search uses the frozen parallel-direction tolerance of `0.10` radians
- axes must be sorted with the principal/highest-order axis first
- axis directions and plane normals are expected to be normalized by the host
- `has_inversion` is passed as a boolean byte
- returned labels are null-terminated UTF-8 stored in a fixed host-owned buffer
- render axis output count equals input axis count
- render plane output count equals input plane count
- render axis extent remains `mol_radius * 1.5`
- render plane square size remains `mol_radius * 1.8`
- the current ABI intentionally does not emit candidate axes or mirror planes
  from the full calculation result
