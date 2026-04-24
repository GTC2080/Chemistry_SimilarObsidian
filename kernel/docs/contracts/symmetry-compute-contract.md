<!-- Reason: This file freezes the first stateless symmetry compute ABI so point-group rules can move out of the Tauri Rust backend without mixing molecule parsing or viewer DTOs into the kernel boundary. -->

# Symmetry Compute Contract

## Scope

This document covers stateless molecular symmetry computation surfaces.

Current surface:

- `kernel_classify_point_group(axes, axis_count, planes, plane_count, has_inversion, out_result)`

## Ownership

- the host owns input axis and plane arrays
- the kernel owns point-group classification rules
- `kernel_symmetry_classification_result` is filled by value and requires no free call
- invalid pointer/count combinations return `KERNEL_ERROR_INVALID_ARGUMENT`

## Host Boundary

The Tauri host keeps:

- molecule text parsing (`XYZ`, `PDB`, `CIF`)
- candidate axis and mirror-plane search
- render geometry DTO construction
- localized UI error wording

The kernel owns:

- point-group decision ordering
- low-symmetry classification (`C_1`, `C_s`, `C_i`)
- cyclic, dihedral, tetrahedral, octahedral, and icosahedral point-group labels

## Frozen Rules

- axes must be sorted with the principal/highest-order axis first
- axis directions and plane normals are expected to be normalized by the host
- `has_inversion` is passed as a boolean byte
- returned labels are null-terminated UTF-8 stored in a fixed host-owned buffer
- the current ABI intentionally does not parse molecule files or emit render geometry
