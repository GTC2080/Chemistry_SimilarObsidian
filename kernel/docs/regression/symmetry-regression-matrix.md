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

## Principal Axes

Required coverage:

- `kernel_compute_symmetry_principal_axes(...)` computes normalized principal axes from a mass-weighted inertia tensor
- principal-axis output includes the molecular line for a two-atom centered molecule
- null atom input, zero atom count, null output, and null atom elements return `KERNEL_ERROR_INVALID_ARGUMENT`
- invalid principal-axis input resets the preallocated 3-axis output buffer before returning

## Candidate Generation

Required coverage:

- `kernel_generate_symmetry_candidate_directions(...)` emits principal axes and Cartesian unit axes
- direction candidate generation deduplicates parallel directions
- direction candidate generation includes atom-vector and same-element pair candidates
- `kernel_generate_symmetry_candidate_planes(...)` emits found axes, principal axes, and Cartesian unit axes
- plane candidate generation deduplicates parallel normals
- candidate generation rejects null atoms, null count output, missing outputs when capacity is nonzero, and too-small capacity

## Operation Search

Required coverage:

- `kernel_find_symmetry_rotation_axes(...)` validates candidate axes against centered atoms
- rotation-axis search emits the matching order for valid candidates
- rotation-axis search sorts found axes by descending order
- `kernel_find_symmetry_mirror_planes(...)` validates candidate plane normals against centered atoms
- mirror-plane search deduplicates parallel normals
- operation search rejects null atoms, null count output, and missing output arrays when capacity is nonzero
- operation search resets output counts before returning invalid-argument status

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
- Tauri Rust principal-axis calculation is a thin C ABI bridge
- Tauri Rust `symmetry::search` delegates candidate generation and operation matching to the kernel
- Tauri Rust `symmetry::render` is a thin C ABI bridge
- full `calculate_symmetry` smoke tests still exercise the kernel classifier, shape analyzer, principal-axis calculation, candidate generation, operation search, and render geometry through the Rust command path
