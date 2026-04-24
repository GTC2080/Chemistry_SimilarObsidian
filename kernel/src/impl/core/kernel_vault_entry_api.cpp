// Reason: This file owns host-facing vault entry mutations that must stay behind the kernel boundary.

#include "kernel/c_api.h"

#include "core/kernel_internal.h"
#include "core/kernel_shared.h"
#include "index/refresh.h"
#include "storage/storage.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

namespace {

bool is_valid_entry_rel_path(const char* rel_path) {
  if (!kernel::core::is_valid_relative_path(rel_path)) {
    return false;
  }

  const std::string normalized = kernel::core::normalize_rel_path(rel_path);
  return !normalized.empty() && normalized != ".";
}

bool is_valid_dest_folder_rel_path(const char* rel_path) {
  if (kernel::core::is_null_or_empty(rel_path)) {
    return true;
  }
  if (!kernel::core::is_valid_relative_path(rel_path)) {
    return false;
  }
  return true;
}

bool is_valid_new_name(const char* new_name) {
  if (kernel::core::is_null_or_empty(new_name)) {
    return false;
  }

  const std::string_view value(new_name);
  if (value == "." || value == "..") {
    return false;
  }
  return value.find('/') == std::string_view::npos &&
         value.find('\\') == std::string_view::npos;
}

std::string normalize_optional_dest_folder(const char* rel_path) {
  if (kernel::core::is_null_or_empty(rel_path)) {
    return {};
  }

  std::string normalized = kernel::core::normalize_rel_path(rel_path);
  if (normalized == ".") {
    normalized.clear();
  }
  return normalized;
}

std::filesystem::path resolve_rel_path(kernel_handle* handle, std::string_view rel_path) {
  return (handle->paths.root / std::filesystem::path(std::string(rel_path))).lexically_normal();
}

std::filesystem::path resolve_optional_dest_folder(
    kernel_handle* handle,
    const std::string& dest_folder_rel_path) {
  if (dest_folder_rel_path.empty()) {
    return handle->paths.root;
  }
  return resolve_rel_path(handle, dest_folder_rel_path);
}

std::string rel_path_for_index(const std::filesystem::path& path) {
  return path.lexically_normal().generic_string();
}

std::error_code refresh_after_file_change(
    kernel_handle* handle,
    std::string_view old_rel_path,
    std::string_view new_rel_path,
    bool is_rename_like) {
  std::lock_guard lock(handle->storage_mutex);
  std::error_code ec = is_rename_like
      ? kernel::index::rename_or_refresh_path(
            handle->storage,
            handle->paths.root,
            old_rel_path,
            new_rel_path)
      : kernel::index::refresh_markdown_path(
            handle->storage,
            handle->paths.root,
            old_rel_path);
  if (ec) {
    return ec;
  }

  std::uint64_t indexed_note_count = 0;
  ec = kernel::storage::count_active_notes(handle->storage, indexed_note_count);
  if (ec) {
    return ec;
  }
  {
    std::lock_guard runtime_lock(handle->runtime_mutex);
    handle->runtime.indexed_note_count = indexed_note_count;
  }
  return {};
}

std::error_code refresh_after_tree_change(kernel_handle* handle) {
  std::lock_guard lock(handle->storage_mutex);
  std::error_code ec =
      kernel::index::full_rescan_markdown_vault(handle->storage, handle->paths.root);
  if (ec) {
    return ec;
  }

  std::uint64_t indexed_note_count = 0;
  ec = kernel::storage::count_active_notes(handle->storage, indexed_note_count);
  if (ec) {
    return ec;
  }
  {
    std::lock_guard runtime_lock(handle->runtime_mutex);
    handle->runtime.indexed_note_count = indexed_note_count;
  }
  return {};
}

kernel_status map_entry_error(const std::error_code& ec) {
  return kernel::core::make_status(kernel::core::map_error(ec));
}

}  // namespace

extern "C" kernel_status kernel_create_folder(
    kernel_handle* handle,
    const char* folder_rel_path) {
  if (handle == nullptr || !is_valid_entry_rel_path(folder_rel_path)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string normalized_rel_path = kernel::core::normalize_rel_path(folder_rel_path);
  const std::filesystem::path target_path = resolve_rel_path(handle, normalized_rel_path);
  const std::filesystem::path parent_path = target_path.parent_path();

  std::error_code ec;
  if (!std::filesystem::exists(parent_path, ec)) {
    return kernel::core::make_status(KERNEL_ERROR_NOT_FOUND);
  }
  if (ec) {
    return map_entry_error(ec);
  }
  if (!std::filesystem::is_directory(parent_path, ec)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  if (ec) {
    return map_entry_error(ec);
  }
  if (std::filesystem::exists(target_path, ec)) {
    return kernel::core::make_status(KERNEL_ERROR_CONFLICT);
  }
  if (ec) {
    return map_entry_error(ec);
  }

  std::filesystem::create_directory(target_path, ec);
  if (ec) {
    return map_entry_error(ec);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_delete_entry(
    kernel_handle* handle,
    const char* target_rel_path) {
  if (handle == nullptr || !is_valid_entry_rel_path(target_rel_path)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string normalized_rel_path = kernel::core::normalize_rel_path(target_rel_path);
  const std::filesystem::path target_path = resolve_rel_path(handle, normalized_rel_path);

  std::error_code ec;
  if (!std::filesystem::exists(target_path, ec)) {
    return kernel::core::make_status(KERNEL_ERROR_NOT_FOUND);
  }
  if (ec) {
    return map_entry_error(ec);
  }

  const bool is_regular_file = std::filesystem::is_regular_file(target_path, ec);
  if (ec) {
    return map_entry_error(ec);
  }
  const bool is_directory = std::filesystem::is_directory(target_path, ec);
  if (ec) {
    return map_entry_error(ec);
  }

  if (is_regular_file) {
    std::filesystem::remove(target_path, ec);
    if (ec) {
      return map_entry_error(ec);
    }
    ec = refresh_after_file_change(handle, normalized_rel_path, {}, false);
  } else if (is_directory) {
    std::filesystem::remove_all(target_path, ec);
    if (ec) {
      return map_entry_error(ec);
    }
    ec = refresh_after_tree_change(handle);
  } else {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  return map_entry_error(ec);
}

extern "C" kernel_status kernel_rename_entry(
    kernel_handle* handle,
    const char* source_rel_path,
    const char* new_name) {
  if (handle == nullptr || !is_valid_entry_rel_path(source_rel_path) ||
      !is_valid_new_name(new_name)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string old_rel_path = kernel::core::normalize_rel_path(source_rel_path);
  const std::filesystem::path source_path = resolve_rel_path(handle, old_rel_path);

  std::error_code ec;
  if (!std::filesystem::exists(source_path, ec)) {
    return kernel::core::make_status(KERNEL_ERROR_NOT_FOUND);
  }
  if (ec) {
    return map_entry_error(ec);
  }

  const bool source_is_directory = std::filesystem::is_directory(source_path, ec);
  if (ec) {
    return map_entry_error(ec);
  }

  std::filesystem::path final_name(new_name);
  if (!source_is_directory && final_name.extension().empty() &&
      !source_path.extension().empty()) {
    final_name += source_path.extension();
  }

  const std::filesystem::path target_path =
      (source_path.parent_path() / final_name).lexically_normal();
  if (target_path == source_path) {
    return kernel::core::make_status(KERNEL_OK);
  }
  if (std::filesystem::exists(target_path, ec)) {
    return kernel::core::make_status(KERNEL_ERROR_CONFLICT);
  }
  if (ec) {
    return map_entry_error(ec);
  }

  const std::filesystem::path old_rel_path_fs(old_rel_path);
  const std::string new_rel_path =
      rel_path_for_index(old_rel_path_fs.parent_path() / final_name);

  std::filesystem::rename(source_path, target_path, ec);
  if (ec) {
    return map_entry_error(ec);
  }

  ec = source_is_directory
      ? refresh_after_tree_change(handle)
      : refresh_after_file_change(handle, old_rel_path, new_rel_path, true);
  return map_entry_error(ec);
}

extern "C" kernel_status kernel_move_entry(
    kernel_handle* handle,
    const char* source_rel_path,
    const char* dest_folder_rel_path) {
  if (handle == nullptr || !is_valid_entry_rel_path(source_rel_path) ||
      !is_valid_dest_folder_rel_path(dest_folder_rel_path)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string old_rel_path = kernel::core::normalize_rel_path(source_rel_path);
  const std::string dest_rel_path = normalize_optional_dest_folder(dest_folder_rel_path);
  const std::filesystem::path source_path = resolve_rel_path(handle, old_rel_path);
  const std::filesystem::path dest_folder_path =
      resolve_optional_dest_folder(handle, dest_rel_path);

  std::error_code ec;
  if (!std::filesystem::exists(source_path, ec)) {
    return kernel::core::make_status(KERNEL_ERROR_NOT_FOUND);
  }
  if (ec) {
    return map_entry_error(ec);
  }
  if (!std::filesystem::is_directory(dest_folder_path, ec)) {
    return kernel::core::make_status(KERNEL_ERROR_NOT_FOUND);
  }
  if (ec) {
    return map_entry_error(ec);
  }

  const bool source_is_directory = std::filesystem::is_directory(source_path, ec);
  if (ec) {
    return map_entry_error(ec);
  }
  if (source_is_directory && dest_folder_path.lexically_normal() == source_path.lexically_normal()) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  if (source_is_directory) {
    const std::filesystem::path relative_to_source =
        dest_folder_path.lexically_relative(source_path);
    if (!relative_to_source.empty() && *relative_to_source.begin() != "..") {
      return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
    }
  }

  const std::filesystem::path target_path =
      (dest_folder_path / source_path.filename()).lexically_normal();
  if (target_path == source_path) {
    return kernel::core::make_status(KERNEL_OK);
  }
  if (std::filesystem::exists(target_path, ec)) {
    return kernel::core::make_status(KERNEL_ERROR_CONFLICT);
  }
  if (ec) {
    return map_entry_error(ec);
  }

  const std::string new_rel_path = dest_rel_path.empty()
      ? source_path.filename().generic_string()
      : rel_path_for_index(std::filesystem::path(dest_rel_path) / source_path.filename());

  std::filesystem::rename(source_path, target_path, ec);
  if (ec) {
    return map_entry_error(ec);
  }

  ec = source_is_directory
      ? refresh_after_tree_change(handle)
      : refresh_after_file_change(handle, old_rel_path, new_rel_path, true);
  return map_entry_error(ec);
}
