// Reason: Own study heatmap allocation and layout separately from progression rules.

#include "core/kernel_product_study.h"

#include "core/kernel_product_study_calendar.h"
#include "core/kernel_shared.h"

#include <cstdint>
#include <new>
#include <string>
#include <unordered_map>

namespace {

constexpr std::size_t kStudyHeatmapWeeks = 26;
constexpr std::size_t kStudyHeatmapDaysPerWeek = 7;

void reset_heatmap_grid_impl(kernel_heatmap_grid* grid) {
  if (grid == nullptr) {
    return;
  }
  if (grid->cells != nullptr) {
    for (std::size_t index = 0; index < grid->count; ++index) {
      delete[] grid->cells[index].date;
      grid->cells[index].date = nullptr;
    }
    delete[] grid->cells;
  }
  grid->cells = nullptr;
  grid->count = 0;
  grid->max_secs = 0;
  grid->weeks = 0;
  grid->days_per_week = 0;
}

}  // namespace

namespace kernel::core::product {

kernel_status build_study_heatmap_grid(
    const kernel_heatmap_day_activity* days,
    const std::size_t day_count,
    const std::int64_t now_epoch_secs,
    kernel_heatmap_grid* out_grid) {
  if (out_grid == nullptr || (day_count > 0 && days == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  reset_heatmap_grid_impl(out_grid);

  std::unordered_map<std::string, std::int64_t> secs_by_date;
  secs_by_date.reserve(day_count);
  for (std::size_t index = 0; index < day_count; ++index) {
    if (days[index].date == nullptr) {
      return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
    }
    secs_by_date[days[index].date] += days[index].active_secs;
  }

  constexpr std::size_t kTotalCells = kStudyHeatmapWeeks * kStudyHeatmapDaysPerWeek;
  out_grid->cells = new (std::nothrow) kernel_heatmap_cell[kTotalCells]{};
  if (out_grid->cells == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  out_grid->count = kTotalCells;
  out_grid->weeks = kStudyHeatmapWeeks;
  out_grid->days_per_week = kStudyHeatmapDaysPerWeek;

  const std::int64_t today_start =
      study_floor_div(now_epoch_secs, kStudySecsPerDay) * kStudySecsPerDay;
  const std::int64_t total_days = static_cast<std::int64_t>(kTotalCells);
  std::int64_t start_date = today_start - (total_days - 1) * kStudySecsPerDay;
  const std::int64_t day_of_week =
      study_positive_mod(study_floor_div(start_date, kStudySecsPerDay) + 3, 7);
  start_date -= day_of_week * kStudySecsPerDay;

  for (std::size_t week = 0; week < kStudyHeatmapWeeks; ++week) {
    for (std::size_t day = 0; day < kStudyHeatmapDaysPerWeek; ++day) {
      const std::size_t cell_index = week * kStudyHeatmapDaysPerWeek + day;
      const std::int64_t ts =
          start_date + static_cast<std::int64_t>(cell_index) * kStudySecsPerDay;
      const std::string date = format_study_date_from_epoch_secs(ts);
      const auto found = secs_by_date.find(date);
      const std::int64_t secs = found == secs_by_date.end() ? 0 : found->second;

      kernel_heatmap_cell& cell = out_grid->cells[cell_index];
      cell.date = kernel::core::duplicate_c_string(date);
      if (cell.date == nullptr) {
        reset_heatmap_grid_impl(out_grid);
        return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
      }
      cell.secs = secs;
      cell.col = week;
      cell.row = day;
      if (secs > out_grid->max_secs) {
        out_grid->max_secs = secs;
      }
    }
  }

  return kernel::core::make_status(KERNEL_OK);
}

void free_study_heatmap_grid(kernel_heatmap_grid* grid) {
  reset_heatmap_grid_impl(grid);
}

}  // namespace kernel::core::product
