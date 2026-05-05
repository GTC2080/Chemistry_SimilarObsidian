// Reason: Own study progression, streak, and heatmap rules away from the public ABI wrapper.

#include "core/kernel_product_study.h"

#include "core/kernel_product_truth.h"
#include "core/kernel_shared.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <limits>
#include <new>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>

namespace {

constexpr double kStudySecsPerExp = 60.0;
constexpr double kStudyBaseExp = 100.0;
constexpr double kStudyGrowthRate = 1.5;
constexpr std::int64_t kStudyAttrExpPerLevel = 50;
constexpr std::int64_t kSecsPerDay = 86400;
constexpr std::size_t kStudyHeatmapWeeks = 26;
constexpr std::size_t kStudyHeatmapDaysPerWeek = 7;
constexpr std::int64_t kStudyWeekLookbackDays = 6;
constexpr std::int64_t kStudyLegacyHeatmapLookbackDays = 179;
constexpr std::size_t kStudyFolderRankLimit = 5;

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

std::string_view extension_from_note_id(std::string_view note_id) {
  const std::size_t dot = note_id.find_last_of('.');
  if (dot == std::string_view::npos || dot + 1 >= note_id.size()) {
    return {};
  }
  return note_id.substr(dot + 1);
}

std::int64_t calc_study_next_level_exp(const std::int64_t level) {
  const double value =
      kStudyBaseExp * std::pow(kStudyGrowthRate, static_cast<double>(level - 1));
  if (!std::isfinite(value) || value > static_cast<double>(std::numeric_limits<std::int64_t>::max())) {
    return std::numeric_limits<std::int64_t>::max();
  }
  return static_cast<std::int64_t>(std::floor(value));
}

std::int64_t study_attr_level(const std::int64_t exp) {
  return std::min<std::int64_t>(99, 1 + exp / kStudyAttrExpPerLevel);
}

std::int64_t study_secs_to_exp(const std::int64_t secs) {
  return static_cast<std::int64_t>(
      std::floor(static_cast<double>(secs) / kStudySecsPerExp));
}

void add_value_for_attr(
    kernel_truth_attribute_values& values,
    std::string_view attr,
    const std::int64_t value) {
  if (attr == "science") {
    values.science += value;
  } else if (attr == "engineering") {
    values.engineering += value;
  } else if (attr == "finance") {
    values.finance += value;
  } else {
    values.creation += value;
  }
}

kernel_truth_attribute_values attribute_levels_from_exp(
    const kernel_truth_attribute_values& exp) {
  return kernel_truth_attribute_values{
      study_attr_level(exp.science),
      study_attr_level(exp.engineering),
      study_attr_level(exp.creation),
      study_attr_level(exp.finance)};
}

std::int64_t total_exp(const kernel_truth_attribute_values& exp) {
  return exp.science + exp.engineering + exp.creation + exp.finance;
}

std::int64_t count_contiguous_study_streak(
    const std::set<std::int64_t>& active_days,
    const std::int64_t today_bucket) {
  std::int64_t streak_days = 0;
  std::int64_t expected = today_bucket;
  while (active_days.contains(expected)) {
    ++streak_days;
    if (expected == std::numeric_limits<std::int64_t>::min()) {
      break;
    }
    --expected;
  }
  return streak_days;
}

std::int64_t floor_div(const std::int64_t value, const std::int64_t divisor) {
  std::int64_t quotient = value / divisor;
  const std::int64_t remainder = value % divisor;
  if (remainder != 0 && ((remainder < 0) != (divisor < 0))) {
    --quotient;
  }
  return quotient;
}

std::int64_t positive_mod(const std::int64_t value, const std::int64_t modulus) {
  const std::int64_t remainder = value % modulus;
  return remainder < 0 ? remainder + modulus : remainder;
}

bool subtract_days(
    const std::int64_t epoch_secs,
    const std::int64_t days,
    std::int64_t* out_epoch_secs) {
  if (out_epoch_secs == nullptr || days < 0) {
    return false;
  }
  if (days > std::numeric_limits<std::int64_t>::max() / kSecsPerDay) {
    return false;
  }

  const std::int64_t delta = days * kSecsPerDay;
  if (epoch_secs < std::numeric_limits<std::int64_t>::min() + delta) {
    return false;
  }
  *out_epoch_secs = epoch_secs - delta;
  return true;
}

std::string format_date_from_epoch_secs(const std::int64_t epoch_secs) {
  const std::int64_t days = floor_div(epoch_secs, kSecsPerDay);
  const std::int64_t z = days + 719468;
  const std::int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const std::uint64_t doe = static_cast<std::uint64_t>(z - era * 146097);
  const std::uint64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  std::int64_t year = static_cast<std::int64_t>(yoe) + era * 400;
  const std::uint64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const std::uint64_t mp = (5 * doy + 2) / 153;
  const std::uint64_t day = doy - (153 * mp + 2) / 5 + 1;
  const std::uint64_t month = mp < 10 ? mp + 3 : mp - 9;
  if (month <= 2) {
    ++year;
  }

  char buffer[16]{};
  std::snprintf(
      buffer,
      sizeof(buffer),
      "%04lld-%02llu-%02llu",
      static_cast<long long>(year),
      static_cast<unsigned long long>(month),
      static_cast<unsigned long long>(day));
  return std::string(buffer);
}

}  // namespace

namespace kernel::core::product {

kernel_status compute_truth_state_from_activity(
    const kernel_study_note_activity* activities,
    const std::size_t activity_count,
    kernel_truth_state_snapshot* out_state) {
  if (out_state == nullptr || (activity_count > 0 && activities == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  *out_state = kernel_truth_state_snapshot{};
  kernel_truth_attribute_values secs{};
  for (std::size_t index = 0; index < activity_count; ++index) {
    const kernel_study_note_activity& activity = activities[index];
    if (activity.note_id == nullptr) {
      return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
    }
    add_value_for_attr(
        secs,
        route_truth_attribute_by_extension(extension_from_note_id(activity.note_id)),
        activity.active_secs);
  }

  kernel_truth_attribute_values exp{
      study_secs_to_exp(secs.science),
      study_secs_to_exp(secs.engineering),
      study_secs_to_exp(secs.creation),
      study_secs_to_exp(secs.finance)};

  std::int64_t level = 1;
  std::int64_t remaining = total_exp(exp);
  std::int64_t next_level_exp = calc_study_next_level_exp(level);
  while (remaining >= next_level_exp && next_level_exp > 0) {
    remaining -= next_level_exp;
    ++level;
    next_level_exp = calc_study_next_level_exp(level);
  }

  out_state->level = level;
  out_state->total_exp = remaining;
  out_state->next_level_exp = next_level_exp;
  out_state->attributes = attribute_levels_from_exp(exp);
  out_state->attribute_exp = exp;
  return kernel::core::make_status(KERNEL_OK);
}

kernel_status compute_study_stats_window(
    const std::int64_t now_epoch_secs,
    const std::int64_t days_back,
    kernel_study_stats_window* out_window) {
  if (out_window == nullptr || days_back <= 0) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_study_stats_window window{};
  window.today_start_epoch_secs = floor_div(now_epoch_secs, kSecsPerDay) * kSecsPerDay;
  window.today_bucket = floor_div(window.today_start_epoch_secs, kSecsPerDay);
  window.folder_rank_limit = kStudyFolderRankLimit;

  if (
      !subtract_days(
          window.today_start_epoch_secs,
          kStudyWeekLookbackDays,
          &window.week_start_epoch_secs) ||
      !subtract_days(
          window.today_start_epoch_secs,
          days_back - 1,
          &window.daily_window_start_epoch_secs) ||
      !subtract_days(
          window.today_start_epoch_secs,
          kStudyLegacyHeatmapLookbackDays,
          &window.heatmap_start_epoch_secs)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  *out_window = window;
  return kernel::core::make_status(KERNEL_OK);
}

kernel_status compute_study_streak_days(
    const std::int64_t* day_buckets,
    const std::size_t day_count,
    const std::int64_t today_bucket,
    std::int64_t* out_streak_days) {
  if (out_streak_days == nullptr || (day_count > 0 && day_buckets == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  *out_streak_days = 0;

  std::set<std::int64_t> active_days;
  for (std::size_t index = 0; index < day_count; ++index) {
    active_days.insert(day_buckets[index]);
  }

  *out_streak_days = count_contiguous_study_streak(active_days, today_bucket);
  return kernel::core::make_status(KERNEL_OK);
}

kernel_status compute_study_streak_days_from_timestamps(
    const std::int64_t* started_at_epoch_secs,
    const std::size_t timestamp_count,
    const std::int64_t today_bucket,
    std::int64_t* out_streak_days) {
  if (out_streak_days == nullptr || (timestamp_count > 0 && started_at_epoch_secs == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  *out_streak_days = 0;

  std::set<std::int64_t> active_days;
  for (std::size_t index = 0; index < timestamp_count; ++index) {
    active_days.insert(floor_div(started_at_epoch_secs[index], kSecsPerDay));
  }

  *out_streak_days = count_contiguous_study_streak(active_days, today_bucket);
  return kernel::core::make_status(KERNEL_OK);
}

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

  const std::int64_t today_start = floor_div(now_epoch_secs, kSecsPerDay) * kSecsPerDay;
  const std::int64_t total_days = static_cast<std::int64_t>(kTotalCells);
  std::int64_t start_date = today_start - (total_days - 1) * kSecsPerDay;
  const std::int64_t day_of_week =
      positive_mod(floor_div(start_date, kSecsPerDay) + 3, 7);
  start_date -= day_of_week * kSecsPerDay;

  for (std::size_t week = 0; week < kStudyHeatmapWeeks; ++week) {
    for (std::size_t day = 0; day < kStudyHeatmapDaysPerWeek; ++day) {
      const std::size_t cell_index = week * kStudyHeatmapDaysPerWeek + day;
      const std::int64_t ts =
          start_date + static_cast<std::int64_t>(cell_index) * kSecsPerDay;
      const std::string date = format_date_from_epoch_secs(ts);
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
