use super::types::{FoundAxis, FoundPlane, RotationAxis, SymmetryPlane, Vec3D};

const KERNEL_OK: i32 = 0;

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
#[derive(Debug, Clone, Copy, Default)]
struct KernelSymmetryRenderAxis {
    vector: [f64; 3],
    center: [f64; 3],
    order: u8,
    start: [f64; 3],
    end: [f64; 3],
}

#[repr(C)]
#[derive(Debug, Clone, Copy, Default)]
struct KernelSymmetryRenderPlane {
    normal: [f64; 3],
    center: [f64; 3],
    vertices: [[f64; 3]; 4],
}

extern "C" {
    fn kernel_build_symmetry_render_geometry(
        axes: *const KernelSymmetryAxisInput,
        axis_count: usize,
        planes: *const KernelSymmetryPlaneInput,
        plane_count: usize,
        mol_radius: f64,
        out_axes: *mut KernelSymmetryRenderAxis,
        out_planes: *mut KernelSymmetryRenderPlane,
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

fn vec3_from_kernel(value: [f64; 3]) -> Vec3D {
    Vec3D {
        x: value[0],
        y: value[1],
        z: value[2],
    }
}

fn axis_from_kernel(axis: &KernelSymmetryRenderAxis) -> RotationAxis {
    RotationAxis {
        vector: vec3_from_kernel(axis.vector),
        center: vec3_from_kernel(axis.center),
        order: axis.order,
        start: vec3_from_kernel(axis.start),
        end: vec3_from_kernel(axis.end),
    }
}

fn plane_from_kernel(plane: &KernelSymmetryRenderPlane) -> SymmetryPlane {
    SymmetryPlane {
        normal: vec3_from_kernel(plane.normal),
        center: vec3_from_kernel(plane.center),
        vertices: plane.vertices.map(vec3_from_kernel),
    }
}

pub(super) fn build_render_geometry(
    axes: &[FoundAxis],
    planes: &[FoundPlane],
    mol_radius: f64,
) -> Result<(Vec<RotationAxis>, Vec<SymmetryPlane>), String> {
    let kernel_axes: Vec<KernelSymmetryAxisInput> = axes.iter().map(axis_to_kernel).collect();
    let kernel_planes: Vec<KernelSymmetryPlaneInput> = planes.iter().map(plane_to_kernel).collect();
    let mut out_axes = vec![KernelSymmetryRenderAxis::default(); kernel_axes.len()];
    let mut out_planes = vec![KernelSymmetryRenderPlane::default(); kernel_planes.len()];

    let status = unsafe {
        kernel_build_symmetry_render_geometry(
            kernel_axes.as_ptr(),
            kernel_axes.len(),
            kernel_planes.as_ptr(),
            kernel_planes.len(),
            mol_radius,
            out_axes.as_mut_ptr(),
            out_planes.as_mut_ptr(),
        )
    };
    if status.code != KERNEL_OK {
        return Err("对称性渲染几何生成失败".to_string());
    }

    Ok((
        out_axes.iter().map(axis_from_kernel).collect(),
        out_planes.iter().map(plane_from_kernel).collect(),
    ))
}

#[cfg(test)]
mod tests {
    use nalgebra::Vector3;

    use super::*;

    #[test]
    fn render_geometry_uses_kernel_axis_extent_and_plane_vertices() {
        let axes = vec![FoundAxis {
            dir: Vector3::z(),
            order: 2,
        }];
        let planes = vec![FoundPlane {
            normal: Vector3::z(),
        }];

        let (render_axes, render_planes) = build_render_geometry(&axes, &planes, 2.0).unwrap();

        assert_eq!(render_axes.len(), 1);
        assert_eq!(render_axes[0].order, 2);
        assert!((render_axes[0].start.z + 3.0).abs() < 1.0e-8);
        assert!((render_axes[0].end.z - 3.0).abs() < 1.0e-8);
        assert_eq!(render_planes.len(), 1);
        assert!((render_planes[0].vertices[0].x + 3.6).abs() < 1.0e-8);
        assert!((render_planes[0].vertices[0].y - 3.6).abs() < 1.0e-8);
    }
}
