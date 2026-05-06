#include "sealed_kernel_bridge_internal.h"

using namespace sealed_kernel_bridge_internal;

int32_t sealed_kernel_bridge_compute_truth_state_json(
    const char* const* note_ids_utf8,
    const int64_t* active_secs,
    uint64_t activity_count,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (
      out_json == nullptr || (activity_count > 0 && note_ids_utf8 == nullptr) ||
      (activity_count > 0 && active_secs == nullptr)) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::vector<std::string> note_ids;
  note_ids.reserve(static_cast<std::size_t>(activity_count));
  std::vector<kernel_study_note_activity> activities;
  activities.reserve(static_cast<std::size_t>(activity_count));
  for (uint64_t index = 0; index < activity_count; ++index) {
    if (note_ids_utf8[index] == nullptr) {
      SetError(out_error, "invalid_argument");
      return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
    }
    note_ids.push_back(Utf8ToActiveCodePage(note_ids_utf8[index]));
    activities.push_back(kernel_study_note_activity{note_ids.back().c_str(), active_secs[index]});
  }

  kernel_truth_state_snapshot state{};
  const kernel_status status = kernel_compute_truth_state_from_activity(
      activities.empty() ? nullptr : activities.data(),
      static_cast<size_t>(activities.size()),
      &state);
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_compute_truth_state_from_activity", out_error);
  }

  std::string json;
  AppendTruthStateJson(json, state);
  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "allocation_failed");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_compute_study_streak_days(
    const int64_t* day_buckets,
    uint64_t day_count,
    int64_t today_bucket,
    int64_t* out_streak_days,
    char** out_error) {
  if (out_streak_days == nullptr || (day_count > 0 && day_buckets == nullptr)) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const kernel_status status = kernel_compute_study_streak_days(
      day_buckets,
      static_cast<size_t>(day_count),
      today_bucket,
      out_streak_days);
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_compute_study_streak_days", out_error);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_compute_study_streak_days_from_timestamps(
    const int64_t* started_at_epoch_secs,
    uint64_t timestamp_count,
    int64_t today_bucket,
    int64_t* out_streak_days,
    char** out_error) {
  if (out_streak_days == nullptr ||
      (timestamp_count > 0 && started_at_epoch_secs == nullptr)) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const kernel_status status = kernel_compute_study_streak_days_from_timestamps(
      started_at_epoch_secs,
      static_cast<size_t>(timestamp_count),
      today_bucket,
      out_streak_days);
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(
        status,
        "kernel_compute_study_streak_days_from_timestamps",
        out_error);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_compute_study_stats_window(
    int64_t now_epoch_secs,
    int64_t days_back,
    int64_t* out_today_start_epoch_secs,
    int64_t* out_today_bucket,
    int64_t* out_week_start_epoch_secs,
    int64_t* out_daily_window_start_epoch_secs,
    int64_t* out_heatmap_start_epoch_secs,
    uint64_t* out_folder_rank_limit,
    char** out_error) {
  if (
      out_today_start_epoch_secs == nullptr || out_today_bucket == nullptr ||
      out_week_start_epoch_secs == nullptr || out_daily_window_start_epoch_secs == nullptr ||
      out_heatmap_start_epoch_secs == nullptr || out_folder_rank_limit == nullptr) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_study_stats_window window{};
  const kernel_status status =
      kernel_compute_study_stats_window(now_epoch_secs, days_back, &window);
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_compute_study_stats_window", out_error);
  }

  *out_today_start_epoch_secs = window.today_start_epoch_secs;
  *out_today_bucket = window.today_bucket;
  *out_week_start_epoch_secs = window.week_start_epoch_secs;
  *out_daily_window_start_epoch_secs = window.daily_window_start_epoch_secs;
  *out_heatmap_start_epoch_secs = window.heatmap_start_epoch_secs;
  *out_folder_rank_limit = static_cast<uint64_t>(window.folder_rank_limit);
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_build_study_heatmap_grid_json(
    const char* const* dates_utf8,
    const int64_t* active_secs,
    uint64_t day_count,
    int64_t now_epoch_secs,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (
      out_json == nullptr || (day_count > 0 && dates_utf8 == nullptr) ||
      (day_count > 0 && active_secs == nullptr)) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::vector<std::string> dates;
  dates.reserve(static_cast<std::size_t>(day_count));
  std::vector<kernel_heatmap_day_activity> days;
  days.reserve(static_cast<std::size_t>(day_count));
  for (uint64_t index = 0; index < day_count; ++index) {
    if (dates_utf8[index] == nullptr) {
      SetError(out_error, "invalid_argument");
      return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
    }
    dates.push_back(Utf8ToActiveCodePage(dates_utf8[index]));
    days.push_back(kernel_heatmap_day_activity{dates.back().c_str(), active_secs[index]});
  }

  kernel_heatmap_grid grid{};
  const kernel_status status = kernel_build_study_heatmap_grid(
      days.empty() ? nullptr : days.data(),
      static_cast<size_t>(days.size()),
      now_epoch_secs,
      &grid);
  if (status.code != KERNEL_OK) {
    kernel_free_study_heatmap_grid(&grid);
    return ReturnKernelError(status, "kernel_build_study_heatmap_grid", out_error);
  }

  std::string json;
  AppendHeatmapGridJson(json, grid);
  kernel_free_study_heatmap_grid(&grid);
  *out_json = CopyString(json);
  if (*out_json == nullptr) {
    SetError(out_error, "allocation_failed");
    return static_cast<int32_t>(KERNEL_ERROR_INTERNAL);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_start_study_session(
    sealed_kernel_bridge_session* session,
    const char* note_id_utf8,
    const char* folder_utf8,
    int64_t* out_session_id,
    char** out_error) {
  if (out_session_id != nullptr) {
    *out_session_id = 0;
  }
  if (
      session == nullptr || session->handle == nullptr || note_id_utf8 == nullptr ||
      out_session_id == nullptr) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const kernel_status status = kernel_start_study_session(
      session->handle,
      note_id_utf8,
      folder_utf8 == nullptr ? "" : folder_utf8,
      out_session_id);
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_start_study_session", out_error);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_tick_study_session(
    sealed_kernel_bridge_session* session,
    int64_t session_id,
    int64_t active_secs,
    char** out_error) {
  if (session == nullptr || session->handle == nullptr) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const kernel_status status =
      kernel_tick_study_session(session->handle, session_id, active_secs);
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_tick_study_session", out_error);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_end_study_session(
    sealed_kernel_bridge_session* session,
    int64_t session_id,
    int64_t active_secs,
    char** out_error) {
  if (session == nullptr || session->handle == nullptr) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const kernel_status status =
      kernel_end_study_session(session->handle, session_id, active_secs);
  if (status.code != KERNEL_OK) {
    return ReturnKernelError(status, "kernel_end_study_session", out_error);
  }
  return static_cast<int32_t>(KERNEL_OK);
}

int32_t sealed_kernel_bridge_query_study_stats_json(
    sealed_kernel_bridge_session* session,
    int64_t now_epoch_secs,
    int64_t days_back,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (session == nullptr || session->handle == nullptr || out_json == nullptr) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_owned_buffer buffer{};
  const kernel_status status =
      kernel_query_study_stats_json(session->handle, now_epoch_secs, days_back, &buffer);
  if (status.code != KERNEL_OK) {
    kernel_free_buffer(&buffer);
    return ReturnKernelError(status, "kernel_query_study_stats_json", out_error);
  }
  return CopyKernelOwnedText(buffer, out_json, out_error);
}

int32_t sealed_kernel_bridge_query_study_truth_state_json(
    sealed_kernel_bridge_session* session,
    int64_t now_epoch_millis,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (session == nullptr || session->handle == nullptr || out_json == nullptr) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_owned_buffer buffer{};
  const kernel_status status =
      kernel_query_study_truth_state_json(session->handle, now_epoch_millis, &buffer);
  if (status.code != KERNEL_OK) {
    kernel_free_buffer(&buffer);
    return ReturnKernelError(status, "kernel_query_study_truth_state_json", out_error);
  }
  return CopyKernelOwnedText(buffer, out_json, out_error);
}

int32_t sealed_kernel_bridge_query_study_heatmap_grid_json(
    sealed_kernel_bridge_session* session,
    int64_t now_epoch_secs,
    char** out_json,
    char** out_error) {
  if (out_json != nullptr) {
    *out_json = nullptr;
  }
  if (session == nullptr || session->handle == nullptr || out_json == nullptr) {
    SetError(out_error, "invalid_argument");
    return static_cast<int32_t>(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_owned_buffer buffer{};
  const kernel_status status =
      kernel_query_study_heatmap_grid_json(session->handle, now_epoch_secs, &buffer);
  if (status.code != KERNEL_OK) {
    kernel_free_buffer(&buffer);
    return ReturnKernelError(status, "kernel_query_study_heatmap_grid_json", out_error);
  }
  return CopyKernelOwnedText(buffer, out_json, out_error);
}


