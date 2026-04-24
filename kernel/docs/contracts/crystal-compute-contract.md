<!-- Reason: This file freezes the first stateless crystal compute ABI so crystal migration can proceed without mixing viewer DTOs, CIF parsing, and numeric kernel rules. -->

# Crystal Compute Contract

Last updated: `2026-04-24`

## Scope

This document covers stateless crystal computation surfaces.

Current surface:

- `kernel_calculate_miller_plane(cell, h, k, l, out_result)`

Current exclusions:

- CIF parsing
- supercell expansion
- symmetry operation expansion
- viewer state
- vault indexing or attachment truth

## Boundary

Frozen rules:

- the surface is handle-free and must not read or write vault state
- Tauri Rust may parse CIF text and own serde command marshalling
- the kernel owns Miller-plane numeric geometry
- output buffers are host-owned and require no kernel free call
- failures return `KERNEL_ERROR_INVALID_ARGUMENT` with a typed
  `kernel_crystal_miller_error`

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

- hosts keep CIF parsing and user-facing localized error text
- hosts must not reimplement Miller-plane geometry
- hosts must preserve the existing Tauri command shape for
  `calculate_miller_plane`
- frontends continue to consume host commands rather than kernel ABI directly
