<!-- Reason: This file records regression obligations for stateless symmetry compute slices as they move from Tauri Rust into the kernel. -->

# Symmetry Regression Matrix

## Atom Parsing

Required coverage:

- `kernel_parse_symmetry_atoms_text(...)` parses `XYZ` atom rows
- `XYZ` parsing normalizes element symbols and returns atomic masses
- `kernel_parse_symmetry_atoms_text(...)` parses `PDB` `ATOM` / `HETATM`
  coordinates
- `PDB` parsing prefers the fixed-width element column when present
- `kernel_parse_symmetry_atoms_text(...)` parses simple `CIF` atom loops
- simple `CIF` fractional coordinates convert to Cartesian coordinates using
  cell parameters
- incomplete `XYZ` input returns `KERNEL_ERROR_INVALID_ARGUMENT` with
  `KERNEL_SYMMETRY_PARSE_ERROR_XYZ_INCOMPLETE`
- unsupported formats return `KERNEL_ERROR_INVALID_ARGUMENT` with
  `KERNEL_SYMMETRY_PARSE_ERROR_UNSUPPORTED_FORMAT`
- `kernel_free_symmetry_atom_list(...)` leaves the atom list empty

## Point-Group Classification

Required coverage:

- `kernel_classify_point_group(...)` classifies a C2 axis with vertical mirror planes as `C_2v`
- `kernel_classify_point_group(...)` classifies perpendicular C2 axes plus a horizontal mirror plane as `D_2h`
- empty axes/planes classify as `C_1`
- mirror-only input classifies as `C_s`
- inversion-only input classifies as `C_i`
- null output returns `KERNEL_ERROR_INVALID_ARGUMENT`
- nonzero axis count with null axes returns `KERNEL_ERROR_INVALID_ARGUMENT`
- nonzero plane count with null planes returns `KERNEL_ERROR_INVALID_ARGUMENT`

## Shape Analysis

Required coverage:

- `kernel_analyze_symmetry_shape(...)` computes mass-weighted center of mass
- shape analysis returns the viewer radius with the frozen minimum of `1.0`
- linear molecules return `is_linear = true`
- linear molecules return a normalized linear axis
- inversion-symmetric molecules return `has_inversion = true`
- nonlinear molecules return `is_linear = false`
- non-inversion molecules return `has_inversion = false`
- null atom input, zero atom count, null output, and null atom elements return `KERNEL_ERROR_INVALID_ARGUMENT`

## Render Geometry

Required coverage:

- `kernel_build_symmetry_render_geometry(...)` preserves axis direction and order
- axis start/end points use the frozen `mol_radius * 1.5` extent
- plane output preserves normal and center
- plane vertices use the frozen `mol_radius * 1.8` square size
- nonzero axis count with null axis input or output returns `KERNEL_ERROR_INVALID_ARGUMENT`
- nonzero plane count with null plane input or output returns `KERNEL_ERROR_INVALID_ARGUMENT`
- negative or non-finite radius returns `KERNEL_ERROR_INVALID_ARGUMENT`
- empty axis/plane input with null outputs is accepted

## Host Bridge

Required coverage:

- Tauri Rust `symmetry::parse` is a thin C ABI bridge
- Tauri Rust `symmetry::classify` is a thin C ABI bridge
- Tauri Rust `symmetry::shape` is a thin C ABI bridge
- Tauri Rust `symmetry::render` is a thin C ABI bridge
- full `calculate_symmetry` smoke tests still exercise the kernel classifier and shape analyzer through the Rust command path
