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

## Host Bridge

Required coverage:

- Tauri Rust `symmetry::parse` is a thin C ABI bridge
- Tauri Rust `symmetry::classify` is a thin C ABI bridge
- full `calculate_symmetry` smoke tests still exercise the kernel classifier through the Rust command path
