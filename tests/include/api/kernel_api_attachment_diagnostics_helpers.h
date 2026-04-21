#pragma once

#include <filesystem>
#include <string>
#include <string_view>

struct AttachmentAnomalySnapshot {
  int missing_attachment_count = 0;
  int orphaned_attachment_count = 0;
  std::string summary = "clean";
};

std::string summarize_attachment_anomalies(
    int missing_attachment_count,
    int orphaned_attachment_count);

std::filesystem::path make_attachment_temp_export_path(std::string_view filename);

AttachmentAnomalySnapshot read_attachment_anomaly_snapshot(
    const std::filesystem::path& db_path);
