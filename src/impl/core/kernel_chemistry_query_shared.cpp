// Reason: This file owns Track 5 Batch 1 chemistry spectrum metadata
// projection so chemistry metadata can land as a focused capability line
// without inflating the existing Track 4 domain registry unit.

#include "core/kernel_chemistry_query_shared.h"

#include "chemistry/chemistry_spectrum_metadata.h"
#include "core/kernel_attachment_query_shared.h"
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

  kernel::platform::ReadFileResult file;
  const std::error_code file_ec =
      kernel::platform::read_file(handle->paths.root / std::filesystem::path(record.rel_path), file);
  if (file_ec) {
    return kernel::core::make_status(kernel::core::map_error(file_ec));
  }
  if (!file.stat.exists || !file.stat.is_regular_file) {
    return kernel::core::make_status(KERNEL_ERROR_NOT_FOUND);
  }

  const std::string attachment_content_revision =
      kernel::vault::compute_content_revision(file.bytes);
  const kernel::chemistry::SpectrumParseResult parsed =
      kernel::chemistry::extract_spectrum_metadata(
          record.rel_path,
          file.bytes,
          attachment_content_revision);
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

}  // namespace kernel::core::chemistry_query
