// Reason: This file defines the narrow internal incremental refresh seam that future watcher events will feed.

#pragma once

#include "storage/storage.h"

#include <filesystem>
#include <stop_token>
#include <system_error>
#include <string_view>
#include <vector>

namespace kernel::index {

std::error_code sync_attachment_refs_for_note(
    kernel::storage::Database& db,
    const std::filesystem::path& vault_root,
    const std::vector<std::string>& attachment_refs);

std::error_code refresh_markdown_path(
    kernel::storage::Database& db,
    const std::filesystem::path& vault_root,
    std::string_view rel_path);
std::error_code rename_or_refresh_path(
    kernel::storage::Database& db,
    const std::filesystem::path& vault_root,
    std::string_view old_rel_path,
    std::string_view new_rel_path);
std::error_code full_rescan_markdown_vault(
    kernel::storage::Database& db,
    const std::filesystem::path& vault_root,
    std::stop_token stop_token = {});
void inject_full_rescan_failures(std::errc error, int remaining_failures);
void inject_full_rescan_delay_ms(int delay_ms, int remaining_delays);
void inject_full_rescan_interrupt_after_refresh_phase(int remaining_interrupts);

}  // namespace kernel::index
