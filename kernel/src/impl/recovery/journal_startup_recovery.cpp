// Reason: This file owns startup recovery and unresolved-save counting built on top of journal scans.

#include "recovery/journal.h"

#include "recovery/journal_internal.h"

#include "index/refresh.h"
#include "parser/parser.h"
#include "platform/platform.h"
#include "vault/revision.h"

namespace kernel::recovery::detail {

std::error_code recover_pending_save(
    const std::filesystem::path& vault_root,
    const PendingSave& pending_save,
    kernel::storage::Database& storage,
    bool* out_recovered_live_note) {
  if (out_recovered_live_note != nullptr) {
    *out_recovered_live_note = false;
  }
  const auto target_path = (vault_root / pending_save.rel_path).lexically_normal();
  kernel::platform::FileStat target_stat{};
  std::error_code ec = kernel::platform::stat_file(target_path, target_stat);
  if (ec) {
    return ec;
  }

  if (target_stat.exists && target_stat.is_regular_file) {
    if (out_recovered_live_note != nullptr) {
      *out_recovered_live_note = true;
    }
    kernel::platform::ReadFileResult file;
    ec = kernel::platform::read_file(target_path, file);
    if (ec) {
      return ec;
    }
    const auto parse_result = kernel::parser::parse_markdown(file.bytes);
    ec = kernel::storage::upsert_note_metadata(
        storage,
        pending_save.rel_path,
        file.stat,
        kernel::vault::compute_content_revision(file.bytes),
        parse_result,
        file.bytes);
    if (ec) {
      return ec;
    }
    ec = kernel::index::sync_attachment_refs_for_note(
        storage,
        vault_root,
        parse_result.attachment_refs);
    if (ec) {
      return ec;
    }
    if (!pending_save.temp_path.empty()) {
      return kernel::platform::remove_file_if_exists(pending_save.temp_path);
    }
    return {};
  }

  if (!pending_save.temp_path.empty()) {
    return kernel::platform::remove_file_if_exists(pending_save.temp_path);
  }

  return {};
}

}  // namespace kernel::recovery::detail

namespace kernel::recovery {

std::error_code count_unfinished_save_operations(
    const std::filesystem::path& journal_path,
    std::uint64_t& out_count) {
  detail::JournalScan scan;
  const std::error_code ec = detail::scan_journal(journal_path, scan);
  if (ec) {
    return ec;
  }
  out_count = static_cast<std::uint64_t>(scan.unfinished_ops.size());
  return {};
}

std::error_code recover_startup(
    const std::filesystem::path& journal_path,
    const std::filesystem::path& vault_root,
    kernel::storage::Database& storage,
    std::uint64_t& out_pending_count,
    StartupRecoverySummary* out_summary) {
  detail::JournalScan scan;
  std::error_code ec = detail::scan_journal(journal_path, scan);
  if (ec) {
    return ec;
  }
  std::size_t recovered_live_note_count = 0;
  std::size_t cleaned_temp_only_count = 0;

  for (const auto& [operation_id, pending_save] : scan.unfinished_ops) {
    bool recovered_live_note = false;
    ec = detail::recover_pending_save(
        vault_root,
        pending_save,
        storage,
        &recovered_live_note);
    if (ec) {
      return ec;
    }
    if (recovered_live_note) {
      ++recovered_live_note_count;
    } else {
      ++cleaned_temp_only_count;
    }
  }

  if (out_summary != nullptr) {
    if (recovered_live_note_count != 0) {
      out_summary->outcome = "recovered_pending_saves";
    } else if (cleaned_temp_only_count != 0) {
      out_summary->outcome = "cleaned_temp_only_pending_saves";
    } else {
      out_summary->outcome = "clean_startup";
    }
    out_summary->detected_corrupt_tail = scan.detected_corrupt_tail;
  }

  out_pending_count = 0;
  return detail::rewrite_journal(journal_path, {});
}

}  // namespace kernel::recovery
