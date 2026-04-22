// Reason: This file defines the narrow one-shot integration from watcher poll results into incremental refresh actions.

#pragma once

#include "storage/storage.h"
#include "watcher/session.h"

#include <filesystem>
#include <stop_token>
#include <system_error>
#include <vector>

namespace kernel::watcher {

std::error_code poll_and_refresh_once(
    WatchSession& session,
    kernel::storage::Database& db,
    DWORD timeout_ms,
    std::vector<CoalescedAction>& out_actions,
    std::stop_token stop_token = {});
std::error_code apply_actions(
    kernel::storage::Database& db,
    const std::filesystem::path& vault_root,
    const std::vector<CoalescedAction>& actions,
    std::stop_token stop_token = {});
void inject_apply_actions_delay_after_count(
    int after_action_count,
    int delay_ms,
    int remaining_delays);

}  // namespace kernel::watcher
