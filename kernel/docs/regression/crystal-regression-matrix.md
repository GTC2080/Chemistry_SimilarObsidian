<!-- Reason: This file records regression obligations for stateless crystal compute slices as they move from Tauri Rust into the kernel. -->

# Crystal Regression Matrix

Last updated: `2026-04-24`

## Stateless Crystal Compute

The repository must retain regression coverage for:

- `kernel_parse_cif_crystal(...)` extracts complete cell parameters
- CIF cell values may be inline or on the next non-comment line
- CIF atom loops extract fractional coordinates and normalized element labels
- CIF symmetry loops parse identity operations
- CIF symmetry loops parse signed fractional translations
- CIF parsing defaults to identity symmetry when no symmetry loop is present
- missing cell parameters report `KERNEL_CRYSTAL_PARSE_ERROR_MISSING_CELL`
- missing fractional atom sites report `KERNEL_CRYSTAL_PARSE_ERROR_MISSING_ATOMS`
- `kernel_free_crystal_parse_result(...)` leaves the result empty and is safe on
  partially filled data
- `kernel_calculate_miller_plane(...)` succeeds for a valid cubic `(100)`
  plane
- cubic `(100)` returns a unit normal along `x`
- cubic `(100)` returns the expected center and plane offset
- cubic `(111)` returns the normalized `[1, 1, 1]` direction
- returned vertices span the visualization plane
- zero Miller index `(0, 0, 0)` returns `KERNEL_ERROR_INVALID_ARGUMENT`
- zero Miller index reports `KERNEL_CRYSTAL_MILLER_ERROR_ZERO_INDEX`
- degenerate `gamma` reports `KERNEL_CRYSTAL_MILLER_ERROR_GAMMA_TOO_SMALL`
- null input and null output pointers are rejected
- Tauri Rust bridge tests continue to cover the existing command-facing
  Miller-plane behavior
- `kernel_build_supercell(...)` expands a simple cubic cell to the expected
  Cartesian coordinates
- `kernel_build_supercell(...)` preserves element labels
- supercell generation deduplicates identical symmetry operations
- supercell generation reports `KERNEL_CRYSTAL_SUPERCELL_ERROR_TOO_MANY_ATOMS`
  and the estimated atom count when expansion exceeds `50000`
- supercell generation reports `KERNEL_CRYSTAL_SUPERCELL_ERROR_GAMMA_TOO_SMALL`
  for degenerate cell geometry
- `kernel_free_supercell_result(...)` leaves the result empty and is safe on
  partially filled data
