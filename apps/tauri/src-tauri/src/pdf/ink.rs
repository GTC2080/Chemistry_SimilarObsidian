//! PDF ink smoothing bridge.
//!
//! Tauri Rust keeps annotation DTOs and command marshalling; Douglas-Peucker
//! simplification and Catmull-Rom interpolation live in the C++ kernel.

use serde::{Deserialize, Serialize};

use crate::pdf::annotations::InkPoint;

const KERNEL_OK: i32 = 0;

#[repr(C)]
#[derive(Debug, Clone, Copy)]
struct KernelStatus {
    code: i32,
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
struct KernelInkPoint {
    x: f32,
    y: f32,
    pressure: f32,
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
struct KernelInkStrokeInput {
    points: *const KernelInkPoint,
    point_count: usize,
    stroke_width: f32,
}

#[repr(C)]
#[derive(Debug)]
struct KernelInkStroke {
    points: *mut KernelInkPoint,
    point_count: usize,
    stroke_width: f32,
}

#[repr(C)]
#[derive(Debug)]
struct KernelInkSmoothingResult {
    strokes: *mut KernelInkStroke,
    count: usize,
}

impl Default for KernelInkSmoothingResult {
    fn default() -> Self {
        Self {
            strokes: std::ptr::null_mut(),
            count: 0,
        }
    }
}

extern "C" {
    fn kernel_smooth_ink_strokes(
        strokes: *const KernelInkStrokeInput,
        stroke_count: usize,
        tolerance: f32,
        out_result: *mut KernelInkSmoothingResult,
    ) -> KernelStatus;
    fn kernel_free_ink_smoothing_result(result: *mut KernelInkSmoothingResult);
}

/// 前端传入的原始笔画
#[derive(Debug, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct RawStroke {
    pub points: Vec<InkPoint>,
    pub stroke_width: f32,
}

/// 后端返回的平滑笔画
#[derive(Debug, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct SmoothedStroke {
    pub points: Vec<InkPoint>,
    pub stroke_width: f32,
}

fn point_to_kernel(point: &InkPoint) -> KernelInkPoint {
    KernelInkPoint {
        x: point.x,
        y: point.y,
        pressure: point.pressure,
    }
}

fn point_from_kernel(point: &KernelInkPoint) -> InkPoint {
    InkPoint {
        x: point.x,
        y: point.y,
        pressure: point.pressure,
    }
}

unsafe fn copy_kernel_points(
    points: *const KernelInkPoint,
    count: usize,
) -> Result<Vec<InkPoint>, String> {
    if count == 0 {
        return Ok(Vec::new());
    }
    if points.is_null() {
        return Err("ink smoothing kernel returned a stroke without points".to_string());
    }
    Ok(std::slice::from_raw_parts(points, count)
        .iter()
        .map(point_from_kernel)
        .collect())
}

unsafe fn strokes_from_kernel(
    raw: &KernelInkSmoothingResult,
) -> Result<Vec<SmoothedStroke>, String> {
    if raw.count == 0 {
        return Ok(Vec::new());
    }
    if raw.strokes.is_null() {
        return Err("ink smoothing kernel returned missing strokes".to_string());
    }

    let raw_strokes = std::slice::from_raw_parts(raw.strokes, raw.count);
    raw_strokes
        .iter()
        .map(|stroke| {
            Ok(SmoothedStroke {
                points: copy_kernel_points(stroke.points, stroke.point_count)?,
                stroke_width: stroke.stroke_width,
            })
        })
        .collect()
}

/// 对一组原始笔画执行：简化 + 平滑。
pub fn smooth_strokes(
    strokes: Vec<RawStroke>,
    tolerance: f32,
) -> Result<Vec<SmoothedStroke>, String> {
    let kernel_points: Vec<Vec<KernelInkPoint>> = strokes
        .iter()
        .map(|stroke| stroke.points.iter().map(point_to_kernel).collect())
        .collect();
    let kernel_strokes: Vec<KernelInkStrokeInput> = strokes
        .iter()
        .zip(kernel_points.iter())
        .map(|(stroke, points)| KernelInkStrokeInput {
            points: points.as_ptr(),
            point_count: points.len(),
            stroke_width: stroke.stroke_width,
        })
        .collect();

    let mut result = KernelInkSmoothingResult::default();
    let status = unsafe {
        kernel_smooth_ink_strokes(
            kernel_strokes.as_ptr(),
            kernel_strokes.len(),
            tolerance,
            &mut result,
        )
    };
    if status.code != KERNEL_OK {
        unsafe { kernel_free_ink_smoothing_result(&mut result) };
        return Err(format!(
            "ink smoothing kernel failed with status {}",
            status.code
        ));
    }

    let strokes = unsafe { strokes_from_kernel(&result) };
    unsafe { kernel_free_ink_smoothing_result(&mut result) };
    strokes
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn smooth_strokes_uses_kernel_bridge() {
        let strokes = vec![RawStroke {
            points: vec![
                InkPoint {
                    x: 0.0,
                    y: 0.0,
                    pressure: 0.5,
                },
                InkPoint {
                    x: 0.5,
                    y: 0.2,
                    pressure: 0.7,
                },
                InkPoint {
                    x: 1.0,
                    y: 0.0,
                    pressure: 0.9,
                },
            ],
            stroke_width: 0.01,
        }];

        let smoothed = smooth_strokes(strokes, 0.001).expect("kernel smoothing result");
        assert_eq!(smoothed.len(), 1);
        assert!(smoothed[0].points.len() > 3);
        assert_eq!(smoothed[0].stroke_width, 0.01);
        assert!((smoothed[0].points[0].x - 0.0).abs() < 1.0e-6);
        assert!((smoothed[0].points.last().unwrap().x - 1.0).abs() < 1.0e-6);
    }

    #[test]
    fn smooth_strokes_preserves_two_point_strokes() {
        let strokes = vec![RawStroke {
            points: vec![
                InkPoint {
                    x: 0.0,
                    y: 0.0,
                    pressure: 0.5,
                },
                InkPoint {
                    x: 1.0,
                    y: 1.0,
                    pressure: 0.8,
                },
            ],
            stroke_width: 0.02,
        }];

        let smoothed = smooth_strokes(strokes, 0.1).expect("kernel smoothing result");
        assert_eq!(smoothed[0].points.len(), 2);
        assert_eq!(smoothed[0].points[1].pressure, 0.8);
    }
}
