// Reason: Expose study product rules through focused C ABI wrappers.

#include "kernel/c_api.h"

#include "core/kernel_product_study.h"

#include <cstdint>

extern "C" kernel_status kernel_compute_truth_state_from_activity(
    const kernel_study_note_activity* activities,
    const std::size_t activity_count,
    kernel_truth_state_snapshot* out_state) {
  return kernel::core::product::compute_truth_state_from_activity(
      activities, activity_count, out_state);
}

extern "C" kernel_status kernel_compute_study_stats_window(
    const std::int64_t now_epoch_secs,
    const std::int64_t days_back,
    kernel_study_stats_window* out_window) {
  return kernel::core::product::compute_study_stats_window(
      now_epoch_secs, days_back, out_window);
}

extern "C" kernel_status kernel_compute_study_streak_days(
    const std::int64_t* day_buckets,
    const std::size_t day_count,
    const std::int64_t today_bucket,
    std::int64_t* out_streak_days) {
  return kernel::core::product::compute_study_streak_days(
      day_buckets, day_count, today_bucket, out_streak_days);
}

extern "C" kernel_status kernel_compute_study_streak_days_from_timestamps(
    const std::int64_t* started_at_epoch_secs,
    const std::size_t timestamp_count,
    const std::int64_t today_bucket,
    std::int64_t* out_streak_days) {
  return kernel::core::product::compute_study_streak_days_from_timestamps(
      started_at_epoch_secs, timestamp_count, today_bucket, out_streak_days);
}

extern "C" kernel_status kernel_build_study_heatmap_grid(
    const kernel_heatmap_day_activity* days,
    const std::size_t day_count,
    const std::int64_t now_epoch_secs,
    kernel_heatmap_grid* out_grid) {
  return kernel::core::product::build_study_heatmap_grid(
      days, day_count, now_epoch_secs, out_grid);
}

extern "C" void kernel_free_study_heatmap_grid(kernel_heatmap_grid* grid) {
  kernel::core::product::free_study_heatmap_grid(grid);
}
