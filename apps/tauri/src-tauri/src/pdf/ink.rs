//! 笔迹平滑算法（CPU 密集，在 Rust 后端处理）
//!
//! 流程：原始触摸点 → Douglas-Peucker 简化 → Catmull-Rom 插值平滑
//! 前端只负责收集原始点，所有数学运算全部在后端完成。

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
#[derive(Debug, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct SmoothedStroke {
    pub points: Vec<InkPoint>,
    pub stroke_width: f32,
}

/// 对一组原始笔画执行：简化 + 平滑
pub fn smooth_strokes(strokes: Vec<RawStroke>, tolerance: f32) -> Vec<SmoothedStroke> {
    strokes
        .into_iter()
        .map(|s| {
            let simplified = douglas_peucker(&s.points, tolerance);
            let smoothed = catmull_rom_interpolate(&simplified, 8);
            SmoothedStroke {
                points: smoothed,
                stroke_width: s.stroke_width,
            }
        })
        .collect()
}

// ---------------------------------------------------------------------------
// Douglas-Peucker 简化：减少冗余点，保留曲线特征
// ---------------------------------------------------------------------------

fn douglas_peucker(points: &[InkPoint], epsilon: f32) -> Vec<InkPoint> {
    if points.len() <= 2 {
        return points.to_vec();
    }

    let first = &points[0];
    let last = &points[points.len() - 1];

    let mut max_dist: f32 = 0.0;
    let mut max_idx = 0;

    for (i, p) in points.iter().enumerate().skip(1).take(points.len() - 2) {
        let d = perpendicular_distance(p, first, last);
        if d > max_dist {
            max_dist = d;
            max_idx = i;
        }
    }

    if max_dist > epsilon {
        let mut left = douglas_peucker(&points[..=max_idx], epsilon);
        let right = douglas_peucker(&points[max_idx..], epsilon);
        left.pop(); // 去掉重复的分割点
        left.extend(right);
        left
    } else {
        vec![first.clone(), last.clone()]
    }
}

fn perpendicular_distance(p: &InkPoint, a: &InkPoint, b: &InkPoint) -> f32 {
    let dx = b.x - a.x;
    let dy = b.y - a.y;
    let len_sq = dx * dx + dy * dy;
    if len_sq < 1e-12 {
        return ((p.x - a.x).powi(2) + (p.y - a.y).powi(2)).sqrt();
    }
    ((dy * p.x - dx * p.y + b.x * a.y - b.y * a.x).abs()) / len_sq.sqrt()
}

// ---------------------------------------------------------------------------
// Catmull-Rom 样条插值：在简化后的控制点之间生成平滑曲线
// ---------------------------------------------------------------------------

fn catmull_rom_interpolate(points: &[InkPoint], segments: usize) -> Vec<InkPoint> {
    if points.len() < 2 {
        return points.to_vec();
    }
    if points.len() == 2 {
        return points.to_vec();
    }

    let mut result = Vec::with_capacity(points.len() * segments);
    let n = points.len();

    for i in 0..n - 1 {
        let p0 = &points[if i == 0 { 0 } else { i - 1 }];
        let p1 = &points[i];
        let p2 = &points[(i + 1).min(n - 1)];
        let p3 = &points[(i + 2).min(n - 1)];

        for s in 0..segments {
            let t = s as f32 / segments as f32;
            let t2 = t * t;
            let t3 = t2 * t;

            let x = 0.5
                * ((2.0 * p1.x)
                    + (-p0.x + p2.x) * t
                    + (2.0 * p0.x - 5.0 * p1.x + 4.0 * p2.x - p3.x) * t2
                    + (-p0.x + 3.0 * p1.x - 3.0 * p2.x + p3.x) * t3);
            let y = 0.5
                * ((2.0 * p1.y)
                    + (-p0.y + p2.y) * t
                    + (2.0 * p0.y - 5.0 * p1.y + 4.0 * p2.y - p3.y) * t2
                    + (-p0.y + 3.0 * p1.y - 3.0 * p2.y + p3.y) * t3);
            let pressure = p1.pressure + (p2.pressure - p1.pressure) * t;

            result.push(InkPoint { x, y, pressure });
        }
    }

    // 加上最后一个点
    result.push(points[n - 1].clone());
    result
}
