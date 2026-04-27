//! PDF ink smoothing DTOs.
//!
//! Tauri Rust keeps annotation DTOs and command marshalling; Douglas-Peucker
//! simplification and Catmull-Rom interpolation live in the C++ kernel.

use serde::{Deserialize, Serialize};

use crate::pdf::annotations::InkPoint;

/// 前端传入的原始笔画
#[derive(Debug, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct RawStroke {
    pub points: Vec<InkPoint>,
    pub stroke_width: f32,
}

/// 后端返回的平滑笔画
#[derive(Debug, Deserialize, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct SmoothedStroke {
    pub points: Vec<InkPoint>,
    pub stroke_width: f32,
}

/// 对一组原始笔画执行：简化 + 平滑。
pub fn smooth_strokes(
    strokes: Vec<RawStroke>,
    tolerance: f32,
) -> Result<Vec<SmoothedStroke>, String> {
    crate::sealed_kernel::smooth_ink_strokes(strokes, tolerance).map_err(|err| err.to_string())
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
