// Reason: This file centralizes API-test-specific helpers for journal framing and settled-state waits.

#pragma once

#include "kernel/c_api.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

std::vector<std::string> read_journal_payloads(const std::filesystem::path& journal_path);
void prepare_state_dir_for_vault(const std::filesystem::path& vault);
void expect_empty_journal_if_present(const std::filesystem::path& journal_path, std::string_view message);

void append_truncated_tail_record(const std::filesystem::path& path);
void append_crc_mismatch_tail_record(const std::filesystem::path& path);

void require_index_ready(kernel_handle* handle, std::string_view message);
void require_attachment_lookup_state(
    kernel_handle* handle,
    const char* attachment_rel_path,
    kernel_attachment_presence expected_presence,
    std::uint64_t expected_ref_count,
    kernel_attachment_kind expected_kind,
    bool expect_nonzero_metadata,
    std::string_view context);
void require_single_note_attachment_ref_state(
    kernel_handle* handle,
    const char* note_rel_path,
    const char* attachment_rel_path,
    kernel_attachment_presence expected_presence,
    std::uint64_t expected_ref_count,
    kernel_attachment_kind expected_kind,
    bool expect_nonzero_metadata,
    std::string_view context);
void require_note_attachment_refs_not_found(
    kernel_handle* handle,
    const char* note_rel_path,
    std::string_view context);
void require_attachment_lookup_not_found(
    kernel_handle* handle,
    const char* attachment_rel_path,
    std::string_view context);
void require_attachment_referrers_not_found(
    kernel_handle* handle,
    const char* attachment_rel_path,
    std::string_view context);
