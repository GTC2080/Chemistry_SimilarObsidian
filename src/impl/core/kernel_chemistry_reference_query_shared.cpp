// Reason: This file owns chemistry note-reference query shaping so the Batch
// 3 ABI wrapper can stay focused on argument validation and marshalling.

#include "core/kernel_chemistry_reference_query_shared.h"

#include "core/kernel_attachment_api_shared.h"
#include "core/kernel_chemistry_reference_resolution.h"
#include "core/kernel_chemistry_query_shared.h"
#include "core/kernel_domain_object_key.h"
#include "storage/storage.h"

namespace kernel::core::chemistry_reference_query {
namespace {

constexpr std::string_view kChemSubtypeNamespace = "chem";
constexpr std::string_view kChemSubtypeName = "spectrum";

kernel_status normalize_required_rel_path(
    const char* rel_path,
    std::string& out_rel_path) {
  return kernel::core::attachment_api::normalize_required_rel_path_argument(
      rel_path,
      out_rel_path);
}

std::string chemistry_domain_object_key(std::string_view attachment_rel_path) {
  return kernel::core::domain_object_key::make_domain_object_key(
      KERNEL_DOMAIN_CARRIER_ATTACHMENT,
      attachment_rel_path,
      kChemSubtypeNamespace,
      kChemSubtypeName);
}

}  // namespace

kernel_status query_note_chem_spectrum_refs(
    kernel_handle* handle,
    const char* note_rel_path,
    const size_t limit,
    std::vector<kernel::core::chemistry_reference_api::ChemSpectrumSourceRefView>& out_refs) {
  std::string normalized_note_rel_path;
  const kernel_status normalized_status =
      normalize_required_rel_path(note_rel_path, normalized_note_rel_path);
  if (normalized_status.code != KERNEL_OK) {
    return normalized_status;
  }

  std::vector<kernel::storage::NoteChemSpectrumSourceRefRecord> raw_records;
  {
    std::lock_guard lock(handle->storage_mutex);
    const std::error_code query_ec =
        kernel::storage::list_note_chem_spectrum_source_ref_records(
            handle->storage,
            normalized_note_rel_path,
            limit,
            raw_records);
    if (query_ec) {
      return kernel::core::make_status(kernel::core::map_error(query_ec));
    }
  }

  out_refs.clear();
  out_refs.reserve(raw_records.size());
  for (const auto& raw_record : raw_records) {
    kernel::core::chemistry_reference_resolution::ResolvedChemSpectrumReference resolved;
    const std::error_code resolve_ec =
        kernel::core::chemistry_reference_resolution::resolve_chem_spectrum_reference(
            handle,
            raw_record.attachment_rel_path,
            raw_record.selector_serialized,
            raw_record.preview_text,
            resolved);
    if (resolve_ec) {
      return kernel::core::make_status(kernel::core::map_error(resolve_ec));
    }

    out_refs.push_back(
        kernel::core::chemistry_reference_api::ChemSpectrumSourceRefView{
            raw_record.attachment_rel_path,
            chemistry_domain_object_key(raw_record.attachment_rel_path),
            resolved.selector_kind,
            raw_record.selector_serialized,
            std::move(resolved.preview_text),
            std::move(resolved.target_basis_revision),
            resolved.state,
            KERNEL_DOMAIN_REF_FLAG_NONE});
  }

  return kernel::core::make_status(KERNEL_OK);
}

kernel_status query_chem_spectrum_referrers(
    kernel_handle* handle,
    const char* attachment_rel_path,
    const size_t limit,
    std::vector<kernel::core::chemistry_reference_api::ChemSpectrumReferrerView>& out_referrers) {
  kernel::core::chemistry_api::ChemSpectrumView spectrum;
  const kernel_status spectrum_status =
      kernel::core::chemistry_query::query_chem_spectrum(handle, attachment_rel_path, spectrum);
  if (spectrum_status.code != KERNEL_OK) {
    return spectrum_status;
  }

  std::string normalized_attachment_rel_path;
  const kernel_status normalized_status =
      normalize_required_rel_path(attachment_rel_path, normalized_attachment_rel_path);
  if (normalized_status.code != KERNEL_OK) {
    return normalized_status;
  }

  std::vector<kernel::storage::ChemSpectrumReferrerRecord> raw_records;
  {
    std::lock_guard lock(handle->storage_mutex);
    const std::error_code query_ec =
        kernel::storage::list_chem_spectrum_referrer_records(
            handle->storage,
            normalized_attachment_rel_path,
            limit,
            raw_records);
    if (query_ec) {
      return kernel::core::make_status(kernel::core::map_error(query_ec));
    }
  }

  out_referrers.clear();
  out_referrers.reserve(raw_records.size());
  for (const auto& raw_record : raw_records) {
    kernel::core::chemistry_reference_resolution::ResolvedChemSpectrumReference resolved;
    const std::error_code resolve_ec =
        kernel::core::chemistry_reference_resolution::resolve_chem_spectrum_reference(
            handle,
            normalized_attachment_rel_path,
            raw_record.selector_serialized,
            raw_record.preview_text,
            resolved);
    if (resolve_ec) {
      return kernel::core::make_status(kernel::core::map_error(resolve_ec));
    }

    out_referrers.push_back(
        kernel::core::chemistry_reference_api::ChemSpectrumReferrerView{
            raw_record.note_rel_path,
            raw_record.note_title,
            normalized_attachment_rel_path,
            chemistry_domain_object_key(normalized_attachment_rel_path),
            resolved.selector_kind,
            raw_record.selector_serialized,
            std::move(resolved.preview_text),
            std::move(resolved.target_basis_revision),
            resolved.state,
            KERNEL_DOMAIN_REF_FLAG_NONE});
  }

  return kernel::core::make_status(KERNEL_OK);
}

}  // namespace kernel::core::chemistry_reference_query
