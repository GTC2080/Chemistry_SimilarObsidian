// Reason: This file keeps chemistry diagnostics sampling separate so Track 5
// can extend support-bundle coverage without bloating the shared storage
// diagnostics unit.

#include "core/kernel_diagnostics_support.h"

#include "core/kernel_chemistry_query_shared.h"
#include "core/kernel_chemistry_reference_resolution.h"
#include "core/kernel_shared.h"
#include "storage/storage.h"

#include <vector>

namespace kernel::core::diagnostics_export {
namespace {

bool chemistry_counts_stable(const kernel_index_state index_state) {
  return index_state != KERNEL_INDEX_CATCHING_UP &&
         index_state != KERNEL_INDEX_REBUILDING;
}

}  // namespace

kernel_status collect_chemistry_diagnostics_snapshot(
    kernel_handle* handle,
    const kernel_index_state index_state,
    ChemistryDiagnosticsSnapshot& out_snapshot) {
  out_snapshot = ChemistryDiagnosticsSnapshot{};
  if (!chemistry_counts_stable(index_state)) {
    return kernel::core::make_status(KERNEL_OK);
  }

  std::vector<kernel::core::chemistry_api::ChemSpectrumView> spectra;
  const kernel_status spectra_status =
      kernel::core::chemistry_query::query_chem_spectra(
          handle,
          static_cast<size_t>(-1),
          spectra);
  if (spectra_status.code != KERNEL_OK) {
    return spectra_status;
  }

  out_snapshot.chemistry_spectra_count =
      static_cast<std::uint64_t>(spectra.size());
  for (const auto& spectrum : spectra) {
    switch (spectrum.state) {
      case KERNEL_DOMAIN_OBJECT_PRESENT:
        ++out_snapshot.chemistry_spectra_present_count;
        break;
      case KERNEL_DOMAIN_OBJECT_MISSING:
        ++out_snapshot.chemistry_spectra_missing_count;
        break;
      case KERNEL_DOMAIN_OBJECT_UNSUPPORTED:
        ++out_snapshot.chemistry_spectra_unsupported_count;
        break;
      case KERNEL_DOMAIN_OBJECT_UNRESOLVED:
      default:
        ++out_snapshot.chemistry_spectra_unresolved_count;
        break;
    }
  }

  std::vector<kernel::storage::NoteChemSpectrumSourceRefRecord> ref_records;
  {
    std::lock_guard storage_lock(handle->storage_mutex);
    const std::error_code records_ec =
        kernel::storage::list_live_chem_spectrum_source_ref_diagnostics_records(
            handle->storage,
            ref_records);
    if (records_ec) {
      return kernel::core::make_status(kernel::core::map_error(records_ec));
    }
  }

  out_snapshot.chemistry_source_ref_count =
      static_cast<std::uint64_t>(ref_records.size());
  for (const auto& record : ref_records) {
    kernel::core::chemistry_reference_resolution::ResolvedChemSpectrumReference
        resolved;
    const std::error_code resolve_ec =
        kernel::core::chemistry_reference_resolution::resolve_chem_spectrum_reference(
            handle,
            record.attachment_rel_path,
            record.selector_serialized,
            record.preview_text,
            resolved);
    if (resolve_ec) {
      return kernel::core::make_status(kernel::core::map_error(resolve_ec));
    }

    switch (resolved.state) {
      case KERNEL_DOMAIN_REF_RESOLVED:
        ++out_snapshot.chemistry_source_ref_resolved_count;
        break;
      case KERNEL_DOMAIN_REF_MISSING:
        ++out_snapshot.chemistry_source_ref_missing_count;
        break;
      case KERNEL_DOMAIN_REF_STALE:
        ++out_snapshot.chemistry_source_ref_stale_count;
        break;
      case KERNEL_DOMAIN_REF_UNSUPPORTED:
        ++out_snapshot.chemistry_source_ref_unsupported_count;
        break;
      case KERNEL_DOMAIN_REF_UNRESOLVED:
      default:
        ++out_snapshot.chemistry_source_ref_unresolved_count;
        break;
    }
  }

  return kernel::core::make_status(KERNEL_OK);
}

}  // namespace kernel::core::diagnostics_export
