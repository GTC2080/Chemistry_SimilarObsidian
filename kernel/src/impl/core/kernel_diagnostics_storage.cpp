// Reason: This file owns storage-backed diagnostics sampling so the main
// export entrypoint does not also carry lock-budget and SQLite summary logic.

#include "core/kernel_diagnostics_support.h"

#include "core/kernel_pdf_reference_resolution.h"
#include "core/kernel_shared.h"
#include "storage/storage.h"

#include <chrono>
#include <mutex>
#include <thread>
#include <utility>

namespace kernel::core::diagnostics_export {
namespace {

constexpr auto kDiagnosticsLockBudget = std::chrono::milliseconds(200);

bool storage_counts_stable(const kernel_index_state index_state) {
  return index_state != KERNEL_INDEX_CATCHING_UP &&
         index_state != KERNEL_INDEX_REBUILDING;
}

template <typename CollectFn>
kernel_status collect_storage_snapshot_locked(
    kernel_handle* handle,
    const kernel_index_state index_state,
    CollectFn&& collect_fn) {
  if (!storage_counts_stable(index_state)) {
    return kernel::core::make_status(KERNEL_OK);
  }

  std::unique_lock<std::mutex> storage_lock(handle->storage_mutex, std::defer_lock);
  const auto lock_deadline = std::chrono::steady_clock::now() + kDiagnosticsLockBudget;
  while (!storage_lock.try_lock()) {
    if (std::chrono::steady_clock::now() >= lock_deadline) {
      return kernel::core::make_status(KERNEL_OK);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }

  return std::forward<CollectFn>(collect_fn)(handle->storage);
}

}  // namespace

kernel_status collect_attachment_diagnostics_snapshot(
    kernel_handle* handle,
    const kernel_index_state index_state,
    AttachmentDiagnosticsSnapshot& out_snapshot) {
  out_snapshot = AttachmentDiagnosticsSnapshot{};
  return collect_storage_snapshot_locked(
      handle,
      index_state,
      [&](kernel::storage::Database& storage) {
        const std::error_code attachment_count_ec =
            kernel::storage::count_attachments(storage, out_snapshot.attachment_count);
        if (attachment_count_ec) {
          return kernel::core::make_status(kernel::core::map_error(attachment_count_ec));
        }

        const std::error_code missing_attachment_count_ec =
            kernel::storage::count_missing_attachments(
                storage,
                out_snapshot.missing_attachment_count);
        if (missing_attachment_count_ec) {
          return kernel::core::make_status(
              kernel::core::map_error(missing_attachment_count_ec));
        }

        const std::error_code orphaned_attachment_count_ec =
            kernel::storage::count_orphaned_attachments(
                storage,
                out_snapshot.orphaned_attachment_count);
        if (orphaned_attachment_count_ec) {
          return kernel::core::make_status(
              kernel::core::map_error(orphaned_attachment_count_ec));
        }

        const std::error_code missing_attachment_paths_ec =
            kernel::storage::list_missing_attachment_paths(
                storage,
                kAttachmentAnomalyPathSummaryLimit,
                out_snapshot.missing_attachment_paths);
        if (missing_attachment_paths_ec) {
          return kernel::core::make_status(
              kernel::core::map_error(missing_attachment_paths_ec));
        }

        const std::error_code orphaned_attachment_paths_ec =
            kernel::storage::list_orphaned_attachment_paths(
                storage,
                kAttachmentAnomalyPathSummaryLimit,
                out_snapshot.orphaned_attachment_paths);
        if (orphaned_attachment_paths_ec) {
          return kernel::core::make_status(
              kernel::core::map_error(orphaned_attachment_paths_ec));
        }

        return kernel::core::make_status(KERNEL_OK);
      });
}

kernel_status collect_pdf_diagnostics_snapshot(
    kernel_handle* handle,
    const kernel_index_state index_state,
    PdfDiagnosticsSnapshot& out_snapshot) {
  out_snapshot = PdfDiagnosticsSnapshot{};
  return collect_storage_snapshot_locked(
      handle,
      index_state,
      [&](kernel::storage::Database& storage) {
        const std::error_code live_count_ec =
            kernel::storage::count_live_pdf_records(storage, out_snapshot.live_pdf_count);
        if (live_count_ec) {
          return kernel::core::make_status(kernel::core::map_error(live_count_ec));
        }

        const std::error_code ready_count_ec =
            kernel::storage::count_live_pdf_records_by_state(
                storage,
                kernel::storage::PdfMetadataState::Ready,
                out_snapshot.ready_pdf_count);
        if (ready_count_ec) {
          return kernel::core::make_status(kernel::core::map_error(ready_count_ec));
        }

        const std::error_code partial_count_ec =
            kernel::storage::count_live_pdf_records_by_state(
                storage,
                kernel::storage::PdfMetadataState::Partial,
                out_snapshot.partial_pdf_count);
        if (partial_count_ec) {
          return kernel::core::make_status(kernel::core::map_error(partial_count_ec));
        }

        const std::error_code invalid_count_ec =
            kernel::storage::count_live_pdf_records_by_state(
                storage,
                kernel::storage::PdfMetadataState::Invalid,
                out_snapshot.invalid_pdf_count);
        if (invalid_count_ec) {
          return kernel::core::make_status(kernel::core::map_error(invalid_count_ec));
        }

        const std::error_code unavailable_count_ec =
            kernel::storage::count_live_pdf_records_by_state(
                storage,
                kernel::storage::PdfMetadataState::Unavailable,
                out_snapshot.unavailable_pdf_count);
        if (unavailable_count_ec) {
          return kernel::core::make_status(
              kernel::core::map_error(unavailable_count_ec));
        }

        const std::error_code anchor_count_ec =
            kernel::storage::count_live_pdf_anchor_records(
                storage,
                out_snapshot.live_pdf_anchor_count);
        if (anchor_count_ec) {
          return kernel::core::make_status(kernel::core::map_error(anchor_count_ec));
        }

        std::vector<kernel::storage::PdfSourceRefDiagnosticsRecord> reference_records;
        const std::error_code reference_records_ec =
            kernel::storage::list_live_pdf_source_ref_diagnostics_records(
                storage,
                reference_records);
        if (reference_records_ec) {
          return kernel::core::make_status(
              kernel::core::map_error(reference_records_ec));
        }

        out_snapshot.pdf_source_ref_count =
            static_cast<std::uint64_t>(reference_records.size());
        for (const auto& record : reference_records) {
          kernel::core::pdf_reference_resolution::ResolvedPdfReference resolved;
          const std::error_code resolve_ec =
              kernel::core::pdf_reference_resolution::resolve_pdf_reference(
                  storage,
                  record.pdf_rel_path,
                  record.anchor_serialized,
                  record.page,
                  record.excerpt_text,
                  resolved);
          if (resolve_ec) {
            return kernel::core::make_status(kernel::core::map_error(resolve_ec));
          }

          switch (resolved.state) {
            case KERNEL_PDF_REF_RESOLVED:
              ++out_snapshot.resolved_pdf_source_ref_count;
              break;
            case KERNEL_PDF_REF_MISSING:
              ++out_snapshot.missing_pdf_source_ref_count;
              break;
            case KERNEL_PDF_REF_STALE:
              ++out_snapshot.stale_pdf_source_ref_count;
              break;
            case KERNEL_PDF_REF_UNRESOLVED:
            default:
              ++out_snapshot.unresolved_pdf_source_ref_count;
              break;
          }
        }

        return kernel::core::make_status(KERNEL_OK);
      });
}

void collect_domain_diagnostics_snapshot(
    const AttachmentDiagnosticsSnapshot& attachment_snapshot,
    const PdfDiagnosticsSnapshot& pdf_snapshot,
    DomainDiagnosticsSnapshot& out_snapshot) {
  out_snapshot = DomainDiagnosticsSnapshot{};
  out_snapshot.attachment_domain_metadata_entry_count =
      attachment_snapshot.attachment_count *
      kernel::core::domain_contract::kAttachmentDomainMetadataPublicKeyCount;
  out_snapshot.pdf_domain_metadata_entry_count =
      pdf_snapshot.live_pdf_count *
      kernel::core::domain_contract::kPdfDomainMetadataPublicKeyCount;
  out_snapshot.domain_object_count =
      attachment_snapshot.attachment_count + pdf_snapshot.live_pdf_count;
  out_snapshot.domain_source_ref_count = pdf_snapshot.pdf_source_ref_count;
  out_snapshot.resolved_domain_source_ref_count =
      pdf_snapshot.resolved_pdf_source_ref_count;
  out_snapshot.missing_domain_source_ref_count =
      pdf_snapshot.missing_pdf_source_ref_count;
  out_snapshot.stale_domain_source_ref_count =
      pdf_snapshot.stale_pdf_source_ref_count;
  out_snapshot.unresolved_domain_source_ref_count =
      pdf_snapshot.unresolved_pdf_source_ref_count;
  out_snapshot.unsupported_domain_source_ref_count = 0;
}

}  // namespace kernel::core::diagnostics_export
