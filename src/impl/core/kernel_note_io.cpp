// Reason: This file owns note read/write commands and the storage/journal workflow they require.

#include "kernel/c_api.h"

#include "core/kernel_internal.h"
#include "core/kernel_shared.h"
#include "index/refresh.h"
#include "parser/parser.h"
#include "platform/platform.h"
#include "recovery/journal.h"
#include "storage/storage.h"
#include "vault/revision.h"

#include <chrono>
#include <filesystem>
#include <string>

namespace {

constexpr auto kWatcherSuppressionTtl = std::chrono::milliseconds(500);

}  // namespace

extern "C" kernel_status kernel_read_note(
    kernel_handle* handle,
    const char* rel_path,
    kernel_owned_buffer* out_buffer,
    kernel_note_metadata* out_metadata) {
  if (handle == nullptr || out_buffer == nullptr || out_metadata == nullptr ||
      !kernel::core::is_valid_relative_path(rel_path)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  out_buffer->data = nullptr;
  out_buffer->size = 0;

  kernel::platform::ReadFileResult file;
  const std::error_code ec =
      kernel::platform::read_file(kernel::core::resolve_note_path(handle, rel_path), file);
  if (ec) {
    return kernel::core::make_status(kernel::core::map_error(ec));
  }

  if (!file.bytes.empty()) {
    auto* owned = new (std::nothrow) char[file.bytes.size()];
    if (owned == nullptr) {
      return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
    }
    std::memcpy(owned, file.bytes.data(), file.bytes.size());
    out_buffer->data = owned;
    out_buffer->size = file.bytes.size();
  }

  kernel::core::fill_metadata(
      file.stat,
      kernel::vault::compute_content_revision(file.bytes),
      out_metadata);
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_write_note(
    kernel_handle* handle,
    const char* rel_path,
    const char* utf8_text,
    size_t text_size,
    const char* expected_revision,
    kernel_note_metadata* out_metadata,
    kernel_write_disposition* out_disposition) {
  if (handle == nullptr || utf8_text == nullptr || out_metadata == nullptr ||
      out_disposition == nullptr || !kernel::core::is_valid_relative_path(rel_path)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const auto target_path = kernel::core::resolve_note_path(handle, rel_path);
  const std::string normalized_rel_path = kernel::core::normalize_rel_path(rel_path);

  kernel::platform::FileStat current_stat{};
  const std::error_code stat_ec = kernel::platform::stat_file(target_path, current_stat);
  if (stat_ec) {
    return kernel::core::make_status(kernel::core::map_error(stat_ec));
  }

  kernel::platform::ReadFileResult current_file;
  std::string current_revision;
  if (current_stat.exists && current_stat.is_regular_file) {
    const std::error_code read_ec = kernel::platform::read_file(target_path, current_file);
    if (read_ec) {
      return kernel::core::make_status(kernel::core::map_error(read_ec));
    }
    current_revision = kernel::vault::compute_content_revision(current_file.bytes);
  }

  if (current_stat.exists) {
    if (kernel::core::is_null_or_empty(expected_revision) || current_revision != expected_revision) {
      return kernel::core::make_status(KERNEL_ERROR_CONFLICT);
    }
  } else if (!kernel::core::is_null_or_empty(expected_revision)) {
    return kernel::core::make_status(KERNEL_ERROR_NOT_FOUND);
  }

  const std::string new_bytes(utf8_text, text_size);
  const std::string new_revision = kernel::vault::compute_content_revision(new_bytes);

  if (current_stat.exists && new_revision == current_revision) {
    kernel::core::fill_metadata(current_file.stat, current_revision, out_metadata);
    *out_disposition = KERNEL_WRITE_NO_OP;
    return kernel::core::make_status(KERNEL_OK);
  }

  {
    std::lock_guard lock(handle->storage_mutex);
    handle->runtime.suppressed_watcher_paths[normalized_rel_path] = WatcherSuppressionEntry{
        std::chrono::steady_clock::now() + kWatcherSuppressionTtl,
        new_revision};
  }

  std::filesystem::path temp_path;
  const std::error_code temp_ec = kernel::platform::write_temp_file(target_path, new_bytes, temp_path);
  if (temp_ec) {
    return kernel::core::make_status(kernel::core::map_error(temp_ec));
  }

  const auto operation_id = kernel::recovery::make_operation_id();
  const std::error_code begin_ec =
      kernel::recovery::append_save_begin(handle->paths.recovery_journal_path, operation_id, rel_path, temp_path);
  if (begin_ec) {
    kernel::platform::remove_file_if_exists(temp_path);
    return kernel::core::make_status(kernel::core::map_error(begin_ec));
  }

  const std::error_code replace_ec = kernel::platform::atomic_replace_file(temp_path, target_path);
  if (replace_ec) {
    kernel::platform::remove_file_if_exists(temp_path);
    return kernel::core::make_status(kernel::core::map_error(replace_ec));
  }

  kernel::platform::ReadFileResult written_file;
  const std::error_code read_back_ec = kernel::platform::read_file(target_path, written_file);
  if (read_back_ec) {
    return kernel::core::make_status(kernel::core::map_error(read_back_ec));
  }

  const std::error_code commit_ec =
      kernel::recovery::append_save_commit(handle->paths.recovery_journal_path, operation_id, rel_path);
  if (commit_ec) {
    return kernel::core::make_status(kernel::core::map_error(commit_ec));
  }

  {
    std::lock_guard lock(handle->storage_mutex);
    handle->runtime.suppressed_watcher_paths[normalized_rel_path] = WatcherSuppressionEntry{
        std::chrono::steady_clock::now() + kWatcherSuppressionTtl,
        new_revision};

    const auto parse_result = kernel::parser::parse_markdown(written_file.bytes);
    const std::error_code note_ec = kernel::storage::upsert_note_metadata(
        handle->storage,
        rel_path,
        written_file.stat,
        kernel::vault::compute_content_revision(written_file.bytes),
        parse_result,
        written_file.bytes);
    if (note_ec) {
      return kernel::core::make_status(kernel::core::map_error(note_ec));
    }

    const std::error_code attachment_ec = kernel::index::sync_attachment_refs_for_note(
        handle->storage,
        handle->paths.root,
        parse_result.attachment_refs);
    if (attachment_ec) {
      return kernel::core::make_status(kernel::core::map_error(attachment_ec));
    }

    const std::error_code mirror_begin_ec = kernel::storage::insert_journal_state(
        handle->storage,
        operation_id,
        "SAVE",
        rel_path,
        temp_path.generic_string(),
        "SAVE_BEGIN");
    if (mirror_begin_ec) {
      return kernel::core::make_status(kernel::core::map_error(mirror_begin_ec));
    }

    const std::error_code mirror_commit_ec = kernel::storage::insert_journal_state(
        handle->storage,
        operation_id,
        "SAVE",
        rel_path,
        "",
        "SAVE_COMMIT");
    if (mirror_commit_ec) {
      return kernel::core::make_status(kernel::core::map_error(mirror_commit_ec));
    }

    std::uint64_t indexed_note_count = 0;
    const std::error_code count_ec =
        kernel::storage::count_active_notes(handle->storage, indexed_note_count);
    if (count_ec) {
      return kernel::core::make_status(kernel::core::map_error(count_ec));
    }
    {
      std::lock_guard runtime_lock(handle->runtime_mutex);
      handle->runtime.indexed_note_count = indexed_note_count;
    }
  }

  std::uint64_t pending_recovery_ops = 0;
  const std::error_code recovery_scan_ec =
      kernel::recovery::count_unfinished_save_operations(
          handle->paths.recovery_journal_path,
          pending_recovery_ops);
  if (recovery_scan_ec) {
    return kernel::core::make_status(kernel::core::map_error(recovery_scan_ec));
  }
  {
    std::lock_guard runtime_lock(handle->runtime_mutex);
    handle->runtime.pending_recovery_ops = pending_recovery_ops;
  }

  kernel::core::fill_metadata(
      written_file.stat,
      kernel::vault::compute_content_revision(written_file.bytes),
      out_metadata);
  *out_disposition = KERNEL_WRITE_WRITTEN;
  return kernel::core::make_status(KERNEL_OK);
}
