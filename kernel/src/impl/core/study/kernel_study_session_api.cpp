// Reason: This file exposes study/session storage as a kernel-owned API so
// Tauri Rust does not carry the legacy SQLite study backend.

#include "core/kernel_shared.h"
#include "storage/storage.h"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <new>
#include <sstream>
#include <string>
#include <vector>

namespace {

bool fill_owned_buffer(std::string_view value, kernel_owned_buffer* out_buffer) {
  if (out_buffer == nullptr) {
    return false;
  }
  *out_buffer = kernel_owned_buffer{};
  auto* data = new (std::nothrow) char[value.size() + 1];
  if (data == nullptr) {
    return false;
  }
  if (!value.empty()) {
    std::memcpy(data, value.data(), value.size());
  }
  data[value.size()] = '\0';
  out_buffer->data = data;
  out_buffer->size = value.size();
  return true;
}

std::int64_t current_epoch_secs() {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

kernel_status invalid_argument() {
  return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
}

kernel_status internal_error() {
  return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
}

kernel_status map_storage_status(const std::error_code& ec) {
  return kernel::core::make_status(kernel::core::map_error(ec));
}

std::string json_attr_values(const kernel_truth_attribute_values& values) {
  std::ostringstream output;
  output << "{"
         << "\"science\":" << values.science << ","
         << "\"engineering\":" << values.engineering << ","
         << "\"creation\":" << values.creation << ","
         << "\"finance\":" << values.finance
         << "}";
  return output.str();
}

std::string build_truth_json(
    const kernel_truth_state_snapshot& state,
    const std::int64_t now_epoch_millis) {
  std::ostringstream output;
  output << "{"
         << "\"level\":" << state.level << ","
         << "\"totalExp\":" << state.total_exp << ","
         << "\"nextLevelExp\":" << state.next_level_exp << ","
         << "\"attributes\":" << json_attr_values(state.attributes) << ","
         << "\"attributeExp\":" << json_attr_values(state.attribute_exp) << ","
         << "\"lastSettlement\":" << now_epoch_millis
         << "}";
  return output.str();
}

std::string build_heatmap_grid_json(const kernel_heatmap_grid& grid) {
  std::ostringstream output;
  output << "{\"cells\":[";
  for (std::size_t index = 0; index < grid.count; ++index) {
    if (index != 0) {
      output << ",";
    }
    const kernel_heatmap_cell& cell = grid.cells[index];
    output << "{"
           << "\"date\":\"" << kernel::core::json_escape(cell.date == nullptr ? "" : cell.date)
           << "\","
           << "\"secs\":" << cell.secs << ","
           << "\"col\":" << cell.col << ","
           << "\"row\":" << cell.row
           << "}";
  }
  output << "],\"maxSecs\":" << grid.max_secs << "}";
  return output.str();
}

std::string build_study_stats_json(
    const kernel::storage::StudyStatsRecords& records,
    const std::int64_t streak_days) {
  std::ostringstream output;
  output << "{"
         << "\"today_active_secs\":" << records.today_active_secs << ","
         << "\"today_files\":" << records.today_files << ","
         << "\"week_active_secs\":" << records.week_active_secs << ","
         << "\"streak_days\":" << streak_days << ",";

  output << "\"daily_summary\":[";
  for (std::size_t index = 0; index < records.daily_summary.size(); ++index) {
    if (index != 0) {
      output << ",";
    }
    const auto& row = records.daily_summary[index];
    output << "{"
           << "\"date\":\"" << kernel::core::json_escape(row.date) << "\","
           << "\"active_secs\":" << row.active_secs << ","
           << "\"file_count\":" << row.file_count
           << "}";
  }
  output << "],";

  output << "\"daily_details\":[";
  std::string current_date;
  bool has_open_group = false;
  bool first_group = true;
  bool first_file = true;
  for (const auto& row : records.daily_details) {
    if (!has_open_group || row.date != current_date) {
      if (has_open_group) {
        output << "]}";
      }
      if (!first_group) {
        output << ",";
      }
      current_date = row.date;
      has_open_group = true;
      first_group = false;
      first_file = true;
      output << "{"
             << "\"date\":\"" << kernel::core::json_escape(current_date) << "\","
             << "\"files\":[";
    }
    if (!first_file) {
      output << ",";
    }
    first_file = false;
    output << "{"
           << "\"note_id\":\"" << kernel::core::json_escape(row.note_id) << "\","
           << "\"folder\":\"" << kernel::core::json_escape(row.folder) << "\","
           << "\"active_secs\":" << row.active_secs
           << "}";
  }
  if (has_open_group) {
    output << "]}";
  }
  output << "],";

  output << "\"folder_ranking\":[";
  for (std::size_t index = 0; index < records.folder_ranking.size(); ++index) {
    if (index != 0) {
      output << ",";
    }
    const auto& row = records.folder_ranking[index];
    output << "{"
           << "\"folder\":\"" << kernel::core::json_escape(row.folder) << "\","
           << "\"total_secs\":" << row.total_secs
           << "}";
  }
  output << "],";

  output << "\"heatmap\":[";
  for (std::size_t index = 0; index < records.heatmap.size(); ++index) {
    if (index != 0) {
      output << ",";
    }
    const auto& row = records.heatmap[index];
    output << "{"
           << "\"date\":\"" << kernel::core::json_escape(row.date) << "\","
           << "\"active_secs\":" << row.active_secs
           << "}";
  }
  output << "]}";
  return output.str();
}

}  // namespace

extern "C" kernel_status kernel_start_study_session(
    kernel_handle* handle,
    const char* note_id,
    const char* folder,
    std::int64_t* out_session_id) {
  if (handle == nullptr || kernel::core::is_null_or_empty(note_id) || out_session_id == nullptr) {
    return invalid_argument();
  }
  *out_session_id = 0;

  std::lock_guard lock(handle->storage_mutex);
  const std::error_code ec = kernel::storage::start_study_session(
      handle->storage,
      note_id,
      folder == nullptr ? "" : folder,
      current_epoch_secs(),
      *out_session_id);
  return map_storage_status(ec);
}

extern "C" kernel_status kernel_tick_study_session(
    kernel_handle* handle,
    const std::int64_t session_id,
    const std::int64_t active_secs) {
  if (handle == nullptr) {
    return invalid_argument();
  }
  std::lock_guard lock(handle->storage_mutex);
  return map_storage_status(
      kernel::storage::add_study_session_active_secs(handle->storage, session_id, active_secs));
}

extern "C" kernel_status kernel_end_study_session(
    kernel_handle* handle,
    const std::int64_t session_id,
    const std::int64_t active_secs) {
  return kernel_tick_study_session(handle, session_id, active_secs);
}

extern "C" kernel_status kernel_query_study_stats_json(
    kernel_handle* handle,
    const std::int64_t now_epoch_secs,
    const std::int64_t days_back,
    kernel_owned_buffer* out_buffer) {
  if (handle == nullptr || out_buffer == nullptr) {
    return invalid_argument();
  }
  *out_buffer = kernel_owned_buffer{};

  kernel_study_stats_window window{};
  kernel_status status = kernel_compute_study_stats_window(now_epoch_secs, days_back, &window);
  if (status.code != KERNEL_OK) {
    return status;
  }

  kernel::storage::StudyStatsRecords records{};
  {
    std::lock_guard lock(handle->storage_mutex);
    const std::error_code ec = kernel::storage::query_study_stats_records(
        handle->storage,
        window.today_start_epoch_secs,
        window.week_start_epoch_secs,
        window.daily_window_start_epoch_secs,
        window.heatmap_start_epoch_secs,
        window.folder_rank_limit,
        records);
    if (ec) {
      return map_storage_status(ec);
    }
  }

  std::int64_t streak_days = 0;
  status = kernel_compute_study_streak_days_from_timestamps(
      records.streak_started_at_epoch_secs.data(),
      records.streak_started_at_epoch_secs.size(),
      window.today_bucket,
      &streak_days);
  if (status.code != KERNEL_OK) {
    return status;
  }

  if (!fill_owned_buffer(build_study_stats_json(records, streak_days), out_buffer)) {
    return internal_error();
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_query_study_truth_state_json(
    kernel_handle* handle,
    const std::int64_t now_epoch_millis,
    kernel_owned_buffer* out_buffer) {
  if (handle == nullptr || out_buffer == nullptr) {
    return invalid_argument();
  }
  *out_buffer = kernel_owned_buffer{};

  std::vector<kernel::storage::StudyNoteActivityRecord> records;
  {
    std::lock_guard lock(handle->storage_mutex);
    const std::error_code ec =
        kernel::storage::query_study_note_activity_records(handle->storage, records);
    if (ec) {
      return map_storage_status(ec);
    }
  }

  std::vector<kernel_study_note_activity> activities;
  activities.reserve(records.size());
  for (const auto& record : records) {
    activities.push_back(kernel_study_note_activity{record.note_id.c_str(), record.active_secs});
  }

  kernel_truth_state_snapshot state{};
  const kernel_status status = kernel_compute_truth_state_from_activity(
      activities.data(),
      activities.size(),
      &state);
  if (status.code != KERNEL_OK) {
    return status;
  }

  if (!fill_owned_buffer(build_truth_json(state, now_epoch_millis), out_buffer)) {
    return internal_error();
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_query_study_heatmap_grid_json(
    kernel_handle* handle,
    const std::int64_t now_epoch_secs,
    kernel_owned_buffer* out_buffer) {
  if (handle == nullptr || out_buffer == nullptr) {
    return invalid_argument();
  }
  *out_buffer = kernel_owned_buffer{};

  std::vector<kernel::storage::StudyHeatmapDayRecord> records;
  {
    std::lock_guard lock(handle->storage_mutex);
    const std::error_code ec =
        kernel::storage::query_all_study_heatmap_day_records(handle->storage, records);
    if (ec) {
      return map_storage_status(ec);
    }
  }

  std::vector<kernel_heatmap_day_activity> days;
  days.reserve(records.size());
  for (const auto& record : records) {
    days.push_back(kernel_heatmap_day_activity{record.date.c_str(), record.active_secs});
  }

  kernel_heatmap_grid grid{};
  const kernel_status status =
      kernel_build_study_heatmap_grid(days.data(), days.size(), now_epoch_secs, &grid);
  if (status.code != KERNEL_OK) {
    kernel_free_study_heatmap_grid(&grid);
    return status;
  }

  const std::string json = build_heatmap_grid_json(grid);
  kernel_free_study_heatmap_grid(&grid);
  if (!fill_owned_buffer(json, out_buffer)) {
    return internal_error();
  }
  return kernel::core::make_status(KERNEL_OK);
}
