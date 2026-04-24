<!-- Reason: This file records regression obligations for stateless crystal compute slices as they move from Tauri Rust into the kernel. -->

# Crystal Regression Matrix

Last updated: `2026-04-24`

## Stateless Crystal Compute

The repository must retain regression coverage for:

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
