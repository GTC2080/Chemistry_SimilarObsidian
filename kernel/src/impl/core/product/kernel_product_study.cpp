// Reason: Own study progression and streak rules away from the public ABI and heatmap layout.

#include "core/kernel_product_study.h"

#include "core/kernel_product_study_calendar.h"
#include "core/kernel_product_truth.h"
#include "core/kernel_shared.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <set>
#include <string_view>

namespace {

constexpr double kStudySecsPerExp = 60.0;
constexpr double kStudyBaseExp = 100.0;
constexpr double kStudyGrowthRate = 1.5;
constexpr std::int64_t kStudyAttrExpPerLevel = 50;
constexpr std::int64_t kStudyWeekLookbackDays = 6;
constexpr std::int64_t kStudyLegacyHeatmapLookbackDays = 179;
constexpr std::size_t kStudyFolderRankLimit = 5;

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
  window.today_start_epoch_secs =
      study_floor_div(now_epoch_secs, kStudySecsPerDay) * kStudySecsPerDay;
  window.today_bucket = study_floor_div(window.today_start_epoch_secs, kStudySecsPerDay);
  window.folder_rank_limit = kStudyFolderRankLimit;

  if (
      !subtract_study_days(
          window.today_start_epoch_secs,
          kStudyWeekLookbackDays,
          &window.week_start_epoch_secs) ||
      !subtract_study_days(
          window.today_start_epoch_secs,
          days_back - 1,
          &window.daily_window_start_epoch_secs) ||
      !subtract_study_days(
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
    active_days.insert(study_floor_div(started_at_epoch_secs[index], kStudySecsPerDay));
  }

  *out_streak_days = count_contiguous_study_streak(active_days, today_bucket);
  return kernel::core::make_status(KERNEL_OK);
}

}  // namespace kernel::core::product
