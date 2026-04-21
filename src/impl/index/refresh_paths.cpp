// Reason: This file owns single-path refresh and rename reconciliation for notes and attachments.

#include "index/refresh.h"

#include "index/refresh_internal.h"

#include "parser/parser.h"
#include "platform/platform.h"
#include "vault/revision.h"

#include <algorithm>
#include <cctype>

namespace kernel::index {

bool is_markdown_rel_path(const std::filesystem::path& path) {
  std::string extension = path.extension().string();
  for (char& ch : extension) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return extension == ".md";
}

bool is_internal_note_temp_rel_path(const std::filesystem::path& path) {
  std::string filename = path.filename().generic_string();
  for (char& ch : filename) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }

  constexpr std::string_view kMarker = ".md.tmp.";
  const std::size_t marker_pos = filename.rfind(kMarker);
  if (marker_pos == std::string::npos) {
    return false;
  }

  const std::string suffix = filename.substr(marker_pos + kMarker.size());
  const std::size_t separator_pos = suffix.find('.');
  if (separator_pos == std::string::npos || separator_pos == 0 ||
      separator_pos + 1 >= suffix.size()) {
    return false;
  }

  const auto digits_only = [](std::string_view value) {
    return std::all_of(value.begin(), value.end(), [](const char ch) {
      return std::isdigit(static_cast<unsigned char>(ch)) != 0;
    });
  };

  return digits_only(std::string_view(suffix).substr(0, separator_pos)) &&
         digits_only(std::string_view(suffix).substr(separator_pos + 1));
}

std::error_code sync_attachment_refs_for_note(
    kernel::storage::Database& db,
    const std::filesystem::path& vault_root,
    const std::vector<std::string>& attachment_refs) {
  for (const auto& attachment_rel_path : attachment_refs) {
    const auto attachment_path =
        (vault_root / std::filesystem::path(attachment_rel_path)).lexically_normal();

    kernel::platform::FileStat attachment_stat{};
    std::error_code ec = kernel::platform::stat_file(attachment_path, attachment_stat);
    if (ec) {
      return ec;
    }

    if (attachment_stat.exists) {
      if (!attachment_stat.is_regular_file) {
        return std::make_error_code(std::errc::invalid_argument);
      }
      ec = kernel::storage::upsert_attachment_metadata(db, attachment_rel_path, attachment_stat);
    } else {
      ec = kernel::storage::mark_attachment_missing(db, attachment_rel_path);
    }
    if (ec) {
      return ec;
    }
  }

  return {};
}

std::error_code refresh_markdown_path(
    kernel::storage::Database& db,
    const std::filesystem::path& vault_root,
    std::string_view rel_path) {
  const std::filesystem::path rel_path_fs{std::string(rel_path)};
  const bool is_markdown = is_markdown_rel_path(rel_path_fs);
  const auto target_path = (vault_root / rel_path_fs).lexically_normal();

  kernel::platform::FileStat stat{};
  std::error_code ec = kernel::platform::stat_file(target_path, stat);
  if (ec) {
    return ec;
  }

  if (!is_markdown) {
    if (is_internal_note_temp_rel_path(rel_path_fs)) {
      return {};
    }
    if (!stat.exists) {
      return kernel::storage::mark_attachment_missing(db, rel_path);
    }
    if (!stat.is_regular_file) {
      return std::make_error_code(std::errc::invalid_argument);
    }
    return kernel::storage::upsert_attachment_metadata(db, rel_path, stat);
  }

  if (!stat.exists) {
    return kernel::storage::mark_note_deleted(db, rel_path);
  }

  if (!stat.is_regular_file) {
    return std::make_error_code(std::errc::invalid_argument);
  }

  kernel::platform::ReadFileResult file;
  ec = kernel::platform::read_file(target_path, file);
  if (ec) {
    return ec;
  }

  const auto parse_result = kernel::parser::parse_markdown(file.bytes);
  ec = kernel::storage::upsert_note_metadata(
      db,
      rel_path,
      file.stat,
      kernel::vault::compute_content_revision(file.bytes),
      parse_result,
      file.bytes);
  if (ec) {
    return ec;
  }

  return sync_attachment_refs_for_note(db, vault_root, parse_result.attachment_refs);
}

std::error_code rename_or_refresh_path(
    kernel::storage::Database& db,
    const std::filesystem::path& vault_root,
    std::string_view old_rel_path,
    std::string_view new_rel_path) {
  const std::filesystem::path old_rel_path_fs{std::string(old_rel_path)};
  const std::filesystem::path new_rel_path_fs{std::string(new_rel_path)};
  const bool old_is_markdown = is_markdown_rel_path(old_rel_path_fs);
  const bool new_is_markdown = is_markdown_rel_path(new_rel_path_fs);

  if (old_is_markdown && new_is_markdown) {
    const auto target_path = (vault_root / new_rel_path_fs).lexically_normal();

    kernel::platform::FileStat stat{};
    std::error_code ec = kernel::platform::stat_file(target_path, stat);
    if (ec) {
      return ec;
    }
    if (stat.exists && stat.is_regular_file) {
      kernel::platform::ReadFileResult file;
      ec = kernel::platform::read_file(target_path, file);
      if (ec) {
        return ec;
      }

      const auto parse_result = kernel::parser::parse_markdown(file.bytes);
      ec = kernel::storage::rename_note_metadata(
          db,
          old_rel_path,
          new_rel_path,
          file.stat,
          kernel::vault::compute_content_revision(file.bytes),
          parse_result,
          file.bytes);
      if (!ec) {
        return sync_attachment_refs_for_note(db, vault_root, parse_result.attachment_refs);
      }
      if (ec != std::make_error_code(std::errc::no_such_file_or_directory) &&
          ec != std::make_error_code(std::errc::file_exists)) {
        return ec;
      }
    }
  }

  std::error_code ec = refresh_markdown_path(db, vault_root, old_rel_path);
  if (ec) {
    return ec;
  }
  return refresh_markdown_path(db, vault_root, new_rel_path);
}

}  // namespace kernel::index
