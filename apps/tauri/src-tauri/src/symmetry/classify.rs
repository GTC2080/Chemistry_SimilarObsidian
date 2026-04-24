use std::ffi::CStr;
use std::os::raw::c_char;

use super::types::{FoundAxis, FoundPlane};

const KERNEL_OK: i32 = 0;
const KERNEL_SYMMETRY_POINT_GROUP_MAX: usize = 32;

#[repr(C)]
#[derive(Debug, Clone, Copy)]
struct KernelStatus {
    code: i32,
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
struct KernelSymmetryAxisInput {
    dir: [f64; 3],
    order: u8,
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
struct KernelSymmetryPlaneInput {
    normal: [f64; 3],
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
struct KernelSymmetryClassificationResult {
    point_group: [c_char; KERNEL_SYMMETRY_POINT_GROUP_MAX],
}

impl Default for KernelSymmetryClassificationResult {
    fn default() -> Self {
        Self {
            point_group: [0; KERNEL_SYMMETRY_POINT_GROUP_MAX],
        }
    }
}

extern "C" {
    fn kernel_classify_point_group(
        axes: *const KernelSymmetryAxisInput,
        axis_count: usize,
        planes: *const KernelSymmetryPlaneInput,
        plane_count: usize,
        has_inversion: u8,
        out_result: *mut KernelSymmetryClassificationResult,
    ) -> KernelStatus;
}

fn axis_to_kernel(axis: &FoundAxis) -> KernelSymmetryAxisInput {
    KernelSymmetryAxisInput {
        dir: [axis.dir.x, axis.dir.y, axis.dir.z],
        order: axis.order,
    }
}

fn plane_to_kernel(plane: &FoundPlane) -> KernelSymmetryPlaneInput {
    KernelSymmetryPlaneInput {
        normal: [plane.normal.x, plane.normal.y, plane.normal.z],
    }
}

pub(super) fn classify_point_group(
    axes: &[FoundAxis],
    planes: &[FoundPlane],
    has_inversion: bool,
) -> String {
    let kernel_axes: Vec<KernelSymmetryAxisInput> = axes.iter().map(axis_to_kernel).collect();
    let kernel_planes: Vec<KernelSymmetryPlaneInput> = planes.iter().map(plane_to_kernel).collect();
    let mut result = KernelSymmetryClassificationResult::default();

    let status = unsafe {
        kernel_classify_point_group(
            kernel_axes.as_ptr(),
            kernel_axes.len(),
            kernel_planes.as_ptr(),
            kernel_planes.len(),
            u8::from(has_inversion),
            &mut result,
        )
    };
    if status.code != KERNEL_OK {
        return "C_1".to_string();
    }

    unsafe {
        CStr::from_ptr(result.point_group.as_ptr())
            .to_string_lossy()
            .into_owned()
    }
}

#[cfg(test)]
mod tests {
    use nalgebra::Vector3;

    use super::*;

    #[test]
    fn classify_point_group_uses_kernel_bridge() {
        let axes = vec![FoundAxis {
            dir: Vector3::z(),
            order: 2,
        }];
        let planes = vec![
            FoundPlane {
                normal: Vector3::x(),
            },
            FoundPlane {
                normal: Vector3::y(),
            },
        ];

        assert_eq!(classify_point_group(&axes, &planes, false), "C_2v");
    }

    #[test]
    fn classify_point_group_falls_back_for_no_symmetry() {
        assert_eq!(classify_point_group(&[], &[], false), "C_1");
    }
}
