// Reason: This file owns Track 5 Batch 1 chemistry spectrum metadata
// projection so chemistry metadata can land as a focused capability line
// without inflating the existing Track 4 domain registry unit.

#include "core/kernel_chemistry_query_shared.h"

#include "chemistry/chemistry_spectrum_metadata.h"
#include "core/kernel_attachment_path_shape.h"
#include "core/kernel_attachment_query_shared.h"
#include "core/kernel_domain_object_key.h"
#include "core/kernel_shared.h"
#include "platform/platform.h"
#include "vault/revision.h"

#include <algorithm>
#include <array>
#include <string_view>

namespace {

struct RegisteredChemistryKey {
  std::string_view key_name;
  kernel_domain_value_kind value_kind;
};

constexpr std::array<RegisteredChemistryKey, 6> kRegisteredChemistryKeys{{
    {"family", KERNEL_DOMAIN_VALUE_TOKEN},
    {"point_count", KERNEL_DOMAIN_VALUE_UINT64},
    {"sample_label", KERNEL_DOMAIN_VALUE_STRING},
    {"source_format", KERNEL_DOMAIN_VALUE_TOKEN},
    {"x_axis_unit", KERNEL_DOMAIN_VALUE_STRING},
    {"y_axis_unit", KERNEL_DOMAIN_VALUE_STRING},
}};

constexpr std::string_view kChemSubtypeNamespace = "chem";
constexpr std::string_view kChemSpectrumSubtypeName = "spectrum";

struct ChemSpectrumCandidate {
  bool is_candidate = false;
  bool is_supported_format = false;
  kernel_chem_spectrum_format source_format = KERNEL_CHEM_SPECTRUM_FORMAT_UNKNOWN;
};

void finalize_entries(
    const size_t limit,
    std::vector<kernel::core::domain_api::DomainMetadataView>& out_entries) {
  std::sort(
      out_entries.begin(),
      out_entries.end(),
      [](const auto& lhs, const auto& rhs) {
        if (lhs.namespace_name != rhs.namespace_name) {
          return lhs.namespace_name < rhs.namespace_name;
        }
        return lhs.key_name < rhs.key_name;
      });
  if (out_entries.size() > limit) {
    out_entries.resize(limit);
  }
}

void append_entry(
    std::vector<kernel::core::domain_api::DomainMetadataView>& out_entries,
    std::string_view carrier_key,
    std::string_view key_name,
    kernel_domain_value_kind value_kind,
    std::string_view string_value,
    const std::uint64_t uint64_value = 0) {
  kernel::core::domain_api::DomainMetadataView entry;
  entry.carrier_kind = KERNEL_DOMAIN_CARRIER_ATTACHMENT;
  entry.carrier_key = std::string(carrier_key);
  entry.namespace_name = std::string(kernel::chemistry::kChemSpectrumNamespace);
  entry.public_schema_revision = kernel::chemistry::kChemSpectrumNamespaceRevision;
  entry.key_name = std::string(key_name);
  entry.value_kind = value_kind;
  entry.flags = KERNEL_DOMAIN_METADATA_FLAG_NONE;
  if (value_kind == KERNEL_DOMAIN_VALUE_UINT64) {
    entry.uint64_value = uint64_value;
  } else {
    entry.string_value = std::string(string_value);
  }
  out_entries.push_back(std::move(entry));
}

void finalize_spectra(
    const size_t limit,
    std::vector<kernel::core::chemistry_api::ChemSpectrumView>& out_spectra) {
  std::sort(
      out_spectra.begin(),
      out_spectra.end(),
      [](const auto& lhs, const auto& rhs) {
        return lhs.attachment_rel_path < rhs.attachment_rel_path;
      });
  if (out_spectra.size() > limit) {
    out_spectra.resize(limit);
  }
}

ChemSpectrumCandidate classify_chem_spectrum_candidate(
    std::string_view rel_path,
    const kernel_attachment_kind coarse_kind) {
  const auto path_shape = kernel::core::attachment_path_shape::describe_attachment_path(rel_path);
  if (path_shape.extension == ".jdx" || path_shape.extension == ".dx") {
    return ChemSpectrumCandidate{
        true,
        true,
        KERNEL_CHEM_SPECTRUM_FORMAT_JCAMP_DX};
  }
  if (path_shape.extension == ".csv") {
    return ChemSpectrumCandidate{
        true,
        true,
        KERNEL_CHEM_SPECTRUM_FORMAT_SPECTRUM_CSV_V1};
  }
  if (coarse_kind == KERNEL_ATTACHMENT_KIND_CHEM_LIKE) {
    return ChemSpectrumCandidate{
        true,
        false,
        KERNEL_CHEM_SPECTRUM_FORMAT_UNKNOWN};
  }
  return ChemSpectrumCandidate{};
}

kernel_status load_supported_spectrum_parse_result(
    kernel_handle* handle,
    const kernel::storage::AttachmentCatalogRecord& record,
    kernel::chemistry::SpectrumParseResult& out_parse_result) {
  kernel::platform::ReadFileResult file;
  const std::error_code file_ec =
      kernel::platform::read_file(handle->paths.root / std::filesystem::path(record.rel_path), file);
  if (file_ec) {
    return kernel::core::make_status(kernel::core::map_error(file_ec));
  }
  if (!file.stat.exists || !file.stat.is_regular_file) {
    out_parse_result = kernel::chemistry::SpectrumParseResult{};
    return kernel::core::make_status(KERNEL_OK);
  }

  const std::string attachment_content_revision =
      kernel::vault::compute_content_revision(file.bytes);
  out_parse_result = kernel::chemistry::extract_spectrum_metadata(
      record.rel_path,
      file.bytes,
      attachment_content_revision);
  return kernel::core::make_status(KERNEL_OK);
}

kernel_status build_chem_spectrum_view(
    kernel_handle* handle,
    const kernel::storage::AttachmentCatalogRecord& record,
    kernel::core::chemistry_api::ChemSpectrumView& out_spectrum,
    bool& out_is_candidate) {
  const auto path_shape =
      kernel::core::attachment_path_shape::describe_attachment_path(record.rel_path);
  const ChemSpectrumCandidate candidate =
      classify_chem_spectrum_candidate(record.rel_path, path_shape.kind);
  out_is_candidate = candidate.is_candidate;
  if (!out_is_candidate) {
    return kernel::core::make_status(KERNEL_OK);
  }

  out_spectrum = kernel::core::chemistry_api::ChemSpectrumView{};
  out_spectrum.attachment_rel_path = record.rel_path;
  out_spectrum.domain_object_key = kernel::core::domain_object_key::make_domain_object_key(
      KERNEL_DOMAIN_CARRIER_ATTACHMENT,
      record.rel_path,
      kChemSubtypeNamespace,
      kChemSpectrumSubtypeName);
  out_spectrum.source_format = candidate.source_format;
  out_spectrum.coarse_kind = path_shape.kind;
  out_spectrum.presence = record.is_missing ? KERNEL_ATTACHMENT_PRESENCE_MISSING
                                            : KERNEL_ATTACHMENT_PRESENCE_PRESENT;
  out_spectrum.flags = KERNEL_DOMAIN_OBJECT_FLAG_NONE;

  if (record.is_missing) {
    out_spectrum.state = KERNEL_DOMAIN_OBJECT_MISSING;
    return kernel::core::make_status(KERNEL_OK);
  }

  if (!candidate.is_supported_format) {
    out_spectrum.state = KERNEL_DOMAIN_OBJECT_UNSUPPORTED;
    return kernel::core::make_status(KERNEL_OK);
  }

  kernel::chemistry::SpectrumParseResult parse_result;
  const kernel_status parse_status =
      load_supported_spectrum_parse_result(handle, record, parse_result);
  if (parse_status.code != KERNEL_OK) {
    return parse_status;
  }

  out_spectrum.state =
      parse_result.status == kernel::chemistry::SpectrumParseStatus::Ready
          ? KERNEL_DOMAIN_OBJECT_PRESENT
          : KERNEL_DOMAIN_OBJECT_UNRESOLVED;
  return kernel::core::make_status(KERNEL_OK);
}

}  // namespace

namespace kernel::core::chemistry_query {

kernel_status query_chem_spectrum_metadata(
    kernel_handle* handle,
    const char* attachment_rel_path,
    const size_t limit,
    std::vector<kernel::core::domain_api::DomainMetadataView>& out_entries) {
  kernel::storage::AttachmentCatalogRecord record;
  const kernel_status record_status =
      kernel::core::attachment_query::query_live_attachment_record(
          handle,
          attachment_rel_path,
          record);
  if (record_status.code != KERNEL_OK) {
    return record_status;
  }

  if (record.is_missing) {
    return kernel::core::make_status(KERNEL_ERROR_NOT_FOUND);
  }

  kernel::chemistry::SpectrumParseResult parsed;
  const kernel_status parse_status =
      load_supported_spectrum_parse_result(handle, record, parsed);
  if (parse_status.code != KERNEL_OK) {
    return parse_status;
  }
  if (parsed.status != kernel::chemistry::SpectrumParseStatus::Ready) {
    return kernel::core::make_status(KERNEL_ERROR_NOT_FOUND);
  }

  out_entries.clear();
  for (const auto& key : kRegisteredChemistryKeys) {
    if (key.key_name == "family") {
      append_entry(
          out_entries,
          record.rel_path,
          key.key_name,
          key.value_kind,
          parsed.metadata.family);
      continue;
    }
    if (key.key_name == "point_count") {
      append_entry(
          out_entries,
          record.rel_path,
          key.key_name,
          key.value_kind,
          "",
          parsed.metadata.point_count);
      continue;
    }
    if (key.key_name == "sample_label") {
      if (!parsed.metadata.sample_label.empty()) {
        append_entry(
            out_entries,
            record.rel_path,
            key.key_name,
            key.value_kind,
            parsed.metadata.sample_label);
      }
      continue;
    }
    if (key.key_name == "source_format") {
      append_entry(
          out_entries,
          record.rel_path,
          key.key_name,
          key.value_kind,
          parsed.metadata.source_format);
      continue;
    }
    if (key.key_name == "x_axis_unit") {
      append_entry(
          out_entries,
          record.rel_path,
          key.key_name,
          key.value_kind,
          parsed.metadata.x_axis_unit);
      continue;
    }
    if (key.key_name == "y_axis_unit") {
      append_entry(
          out_entries,
          record.rel_path,
          key.key_name,
          key.value_kind,
          parsed.metadata.y_axis_unit);
      continue;
    }
  }

  finalize_entries(limit, out_entries);
  return kernel::core::make_status(KERNEL_OK);
}

kernel_status query_chem_spectra(
    kernel_handle* handle,
    const size_t limit,
    std::vector<kernel::core::chemistry_api::ChemSpectrumView>& out_spectra) {
  std::vector<kernel::storage::AttachmentCatalogRecord> records;
  const kernel_status records_status =
      kernel::core::attachment_query::query_live_attachment_list(
          handle,
          static_cast<size_t>(-1),
          records);
  if (records_status.code != KERNEL_OK) {
    return records_status;
  }

  out_spectra.clear();
  for (const auto& record : records) {
    kernel::core::chemistry_api::ChemSpectrumView spectrum;
    bool is_candidate = false;
    const kernel_status build_status =
        build_chem_spectrum_view(handle, record, spectrum, is_candidate);
    if (build_status.code != KERNEL_OK) {
      return build_status;
    }
    if (is_candidate) {
      out_spectra.push_back(std::move(spectrum));
    }
  }

  finalize_spectra(limit, out_spectra);
  return kernel::core::make_status(KERNEL_OK);
}

kernel_status query_chem_spectrum(
    kernel_handle* handle,
    const char* attachment_rel_path,
    kernel::core::chemistry_api::ChemSpectrumView& out_spectrum) {
  kernel::storage::AttachmentCatalogRecord record;
  const kernel_status record_status =
      kernel::core::attachment_query::query_live_attachment_record(
          handle,
          attachment_rel_path,
          record);
  if (record_status.code != KERNEL_OK) {
    return record_status;
  }

  bool is_candidate = false;
  const kernel_status build_status =
      build_chem_spectrum_view(handle, record, out_spectrum, is_candidate);
  if (build_status.code != KERNEL_OK) {
    return build_status;
  }
  if (!is_candidate) {
    return kernel::core::make_status(KERNEL_ERROR_NOT_FOUND);
  }
  return kernel::core::make_status(KERNEL_OK);
}

}  // namespace kernel::core::chemistry_query
