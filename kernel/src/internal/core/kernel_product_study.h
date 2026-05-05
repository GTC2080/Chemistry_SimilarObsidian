// Reason: Keep study progression and heatmap rules out of the product public ABI wrapper.

#pragma once

#include "kernel/c_api.h"

#include <cstddef>
#include <cstdint>

namespace kernel::core::product {

kernel_status compute_truth_state_from_activity(
    const kernel_study_note_activity* activities,
    std::size_t activity_count,
    kernel_truth_state_snapshot* out_state);

kernel_status compute_study_stats_window(
    std::int64_t now_epoch_secs,
    std::int64_t days_back,
    kernel_study_stats_window* out_window);

kernel_status compute_study_streak_days(
    const std::int64_t* day_buckets,
    std::size_t day_count,
    std::int64_t today_bucket,
    std::int64_t* out_streak_days);

kernel_status compute_study_streak_days_from_timestamps(
    const std::int64_t* started_at_epoch_secs,
    std::size_t timestamp_count,
    std::int64_t today_bucket,
    std::int64_t* out_streak_days);

kernel_status build_study_heatmap_grid(
    const kernel_heatmap_day_activity* days,
    std::size_t day_count,
    std::int64_t now_epoch_secs,
    kernel_heatmap_grid* out_grid);

void free_study_heatmap_grid(kernel_heatmap_grid* grid);

}  // namespace kernel::core::product
