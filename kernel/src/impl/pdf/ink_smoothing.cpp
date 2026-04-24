// Reason: This file owns PDF ink simplification and smoothing formerly
// implemented in the Tauri Rust backend.

#include "pdf/ink_smoothing.h"

#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

namespace kernel::pdf {
namespace {

float perpendicular_distance(
    const kernel_ink_point& point,
    const kernel_ink_point& first,
    const kernel_ink_point& last) {
  const float dx = last.x - first.x;
  const float dy = last.y - first.y;
  const float len_sq = dx * dx + dy * dy;
  if (len_sq < 1.0e-12F) {
    const float px = point.x - first.x;
    const float py = point.y - first.y;
    return std::sqrt(px * px + py * py);
  }
  return std::abs(dy * point.x - dx * point.y + last.x * first.y - last.y * first.x) /
         std::sqrt(len_sq);
}

std::vector<kernel_ink_point> douglas_peucker(
    const std::vector<kernel_ink_point>& points,
    const float epsilon) {
  if (points.size() <= 2) {
    return points;
  }

  const auto& first = points.front();
  const auto& last = points.back();
  float max_dist = 0.0F;
  std::size_t max_index = 0;

  for (std::size_t index = 1; index + 1 < points.size(); ++index) {
    const float distance = perpendicular_distance(points[index], first, last);
    if (distance > max_dist) {
      max_dist = distance;
      max_index = index;
    }
  }

  if (max_dist > epsilon) {
    std::vector<kernel_ink_point> left_input(points.begin(), points.begin() + max_index + 1);
    std::vector<kernel_ink_point> right_input(points.begin() + max_index, points.end());
    std::vector<kernel_ink_point> left = douglas_peucker(left_input, epsilon);
    std::vector<kernel_ink_point> right = douglas_peucker(right_input, epsilon);
    if (!left.empty()) {
      left.pop_back();
    }
    left.insert(left.end(), right.begin(), right.end());
    return left;
  }

  return {first, last};
}

std::vector<kernel_ink_point> catmull_rom_interpolate(
    const std::vector<kernel_ink_point>& points,
    const std::size_t segments) {
  if (points.size() < 3) {
    return points;
  }

  std::vector<kernel_ink_point> result;
  result.reserve(points.size() * segments);
  const std::size_t count = points.size();

  for (std::size_t index = 0; index + 1 < count; ++index) {
    const auto& p0 = points[index == 0 ? 0 : index - 1];
    const auto& p1 = points[index];
    const auto& p2 = points[(index + 1 < count) ? index + 1 : count - 1];
    const auto& p3 = points[(index + 2 < count) ? index + 2 : count - 1];

    for (std::size_t segment = 0; segment < segments; ++segment) {
      const float t = static_cast<float>(segment) / static_cast<float>(segments);
      const float t2 = t * t;
      const float t3 = t2 * t;

      kernel_ink_point out{};
      out.x = 0.5F *
          ((2.0F * p1.x) + (-p0.x + p2.x) * t +
           (2.0F * p0.x - 5.0F * p1.x + 4.0F * p2.x - p3.x) * t2 +
           (-p0.x + 3.0F * p1.x - 3.0F * p2.x + p3.x) * t3);
      out.y = 0.5F *
          ((2.0F * p1.y) + (-p0.y + p2.y) * t +
           (2.0F * p0.y - 5.0F * p1.y + 4.0F * p2.y - p3.y) * t2 +
           (-p0.y + 3.0F * p1.y - 3.0F * p2.y + p3.y) * t3);
      out.pressure = p1.pressure + (p2.pressure - p1.pressure) * t;
      result.push_back(out);
    }
  }

  result.push_back(points.back());
  return result;
}

std::vector<kernel_ink_point> copy_points(const kernel_ink_stroke_input& stroke) {
  if (stroke.points == nullptr || stroke.point_count == 0) {
    return {};
  }
  return std::vector<kernel_ink_point>(stroke.points, stroke.points + stroke.point_count);
}

}  // namespace

std::vector<SmoothedInkStroke> smooth_ink_strokes(
    const kernel_ink_stroke_input* strokes,
    const std::size_t stroke_count,
    const float tolerance) {
  std::vector<SmoothedInkStroke> result;
  result.reserve(stroke_count);

  for (std::size_t index = 0; index < stroke_count; ++index) {
    const auto& stroke = strokes[index];
    const std::vector<kernel_ink_point> source_points = copy_points(stroke);
    const std::vector<kernel_ink_point> simplified = douglas_peucker(source_points, tolerance);
    SmoothedInkStroke out{};
    out.stroke_width = stroke.stroke_width;
    out.points = catmull_rom_interpolate(simplified, 8);
    result.push_back(std::move(out));
  }

  return result;
}

}  // namespace kernel::pdf
