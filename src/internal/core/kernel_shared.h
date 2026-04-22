// Reason: This file centralizes the small internal helpers shared by the split kernel implementation files.

#pragma once

#include "core/kernel_internal.h"
#include "kernel/c_api.h"
#include "platform/platform.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

namespace kernel::core {

kernel_status make_status(kernel_error_code code);
kernel_error_code map_error(const std::error_code& ec);

bool is_null_or_empty(const char* value);
bool is_valid_relative_path(const char* rel_path);

std::filesystem::path resolve_note_path(kernel_handle* handle, const char* rel_path);
std::string normalize_rel_path(std::string_view rel_path);
std::uint64_t now_ns();
void clear_index_fault(kernel_handle* handle);
void set_index_fault(kernel_handle* handle, std::string_view reason, int code);
void record_recent_event(
    kernel_handle* handle,
    std::string_view kind,
    std::string_view detail,
    int code = 0);
void record_rebuild_started(kernel_handle* handle, std::string_view detail);
void record_rebuild_result(
    kernel_handle* handle,
    std::string_view result,
    std::uint64_t started_at_ns,
    std::uint64_t completed_at_ns,
    int code = 0);
void record_attachment_recount(
    kernel_handle* handle,
    std::string_view reason,
    std::uint64_t at_ns = 0);
void record_pdf_recount(
    kernel_handle* handle,
    std::string_view reason,
    std::uint64_t at_ns = 0);
void record_domain_recount(
    kernel_handle* handle,
    std::string_view reason,
    std::uint64_t at_ns = 0);
void record_continuity_fallback(kernel_handle* handle, std::string_view reason);

const char* session_state_name(kernel_session_state state);
const char* index_state_name(kernel_index_state state);
std::string json_escape(std::string_view value);

void copy_revision(const std::string& revision, kernel_note_metadata* out_metadata);
void fill_metadata(
    const kernel::platform::FileStat& stat,
    const std::string& revision,
    kernel_note_metadata* out_metadata);

char* duplicate_c_string(std::string_view value);
void free_search_results_impl(kernel_search_results* results);

}  // namespace kernel::core
