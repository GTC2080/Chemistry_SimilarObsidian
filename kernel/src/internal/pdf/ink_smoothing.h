// Reason: This file isolates stateless PDF ink smoothing in the kernel while
// hosts keep annotation DTOs and command marshalling.

#pragma once

#include "kernel/types.h"

#include <cstddef>
#include <vector>

namespace kernel::pdf {

struct SmoothedInkStroke {
  std::vector<kernel_ink_point> points;
  float stroke_width = 0.0F;
};

std::vector<SmoothedInkStroke> smooth_ink_strokes(
    const kernel_ink_stroke_input* strokes,
    std::size_t stroke_count,
    float tolerance);

}  // namespace kernel::pdf
