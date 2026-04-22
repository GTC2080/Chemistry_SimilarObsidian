// Reason: This file owns Track 4 Batch 1 registry-driven domain-metadata
// projection so the first domain surface can stay thin and avoid new truth
// tables before a capability track actually needs them.

#include "core/kernel_domain_query_shared.h"

#include "core/kernel_attachment_query_shared.h"
#include "core/kernel_attachment_path_shape.h"
#include "core/kernel_pdf_query_shared.h"
#include "core/kernel_shared.h"

#include <algorithm>
#include <array>
#include <string_view>

namespace {

enum class DomainValueSource : std::uint8_t {
  AttachmentCarrierSurface = 0,
  AttachmentCoarseKind = 1,
  AttachmentPresence = 2,
  PdfCarrierSurface = 3,
  PdfPresence = 4,
  PdfMetadataState = 5,
  PdfDocTitleState = 6,
  PdfTextLayerState = 7,
  PdfPageCount = 8,
  PdfHasOutline = 9
};

struct RegisteredDomainKey {
  kernel_domain_carrier_kind carrier_kind;
  std::string_view namespace_name;
  std::string_view key_name;
  std::uint32_t public_schema_revision;
  kernel_domain_value_kind value_kind;
  DomainValueSource value_source;
};

constexpr std::array<RegisteredDomainKey, 10> kRegisteredDomainKeys{{
    {KERNEL_DOMAIN_CARRIER_ATTACHMENT,
     "generic",
     "carrier_surface",
     kernel::core::domain_api::kGenericNamespaceRevision,
     KERNEL_DOMAIN_VALUE_TOKEN,
     DomainValueSource::AttachmentCarrierSurface},
    {KERNEL_DOMAIN_CARRIER_ATTACHMENT,
     "generic",
     "coarse_kind",
     kernel::core::domain_api::kGenericNamespaceRevision,
     KERNEL_DOMAIN_VALUE_TOKEN,
     DomainValueSource::AttachmentCoarseKind},
    {KERNEL_DOMAIN_CARRIER_ATTACHMENT,
     "generic",
     "presence",
     kernel::core::domain_api::kGenericNamespaceRevision,
     KERNEL_DOMAIN_VALUE_TOKEN,
     DomainValueSource::AttachmentPresence},
    {KERNEL_DOMAIN_CARRIER_PDF,
     "generic",
     "carrier_surface",
     kernel::core::domain_api::kGenericNamespaceRevision,
     KERNEL_DOMAIN_VALUE_TOKEN,
     DomainValueSource::PdfCarrierSurface},
    {KERNEL_DOMAIN_CARRIER_PDF,
     "generic",
     "doc_title_state",
     kernel::core::domain_api::kGenericNamespaceRevision,
     KERNEL_DOMAIN_VALUE_TOKEN,
     DomainValueSource::PdfDocTitleState},
    {KERNEL_DOMAIN_CARRIER_PDF,
     "generic",
     "has_outline",
     kernel::core::domain_api::kGenericNamespaceRevision,
     KERNEL_DOMAIN_VALUE_BOOL,
     DomainValueSource::PdfHasOutline},
    {KERNEL_DOMAIN_CARRIER_PDF,
     "generic",
     "metadata_state",
     kernel::core::domain_api::kGenericNamespaceRevision,
     KERNEL_DOMAIN_VALUE_TOKEN,
     DomainValueSource::PdfMetadataState},
    {KERNEL_DOMAIN_CARRIER_PDF,
     "generic",
     "page_count",
     kernel::core::domain_api::kGenericNamespaceRevision,
     KERNEL_DOMAIN_VALUE_UINT64,
     DomainValueSource::PdfPageCount},
    {KERNEL_DOMAIN_CARRIER_PDF,
     "generic",
     "presence",
     kernel::core::domain_api::kGenericNamespaceRevision,
     KERNEL_DOMAIN_VALUE_TOKEN,
     DomainValueSource::PdfPresence},
    {KERNEL_DOMAIN_CARRIER_PDF,
     "generic",
     "text_layer_state",
     kernel::core::domain_api::kGenericNamespaceRevision,
     KERNEL_DOMAIN_VALUE_TOKEN,
     DomainValueSource::PdfTextLayerState},
}};

std::string_view attachment_kind_token(const kernel_attachment_kind kind) {
  switch (kind) {
    case KERNEL_ATTACHMENT_KIND_GENERIC_FILE:
      return "generic_file";
    case KERNEL_ATTACHMENT_KIND_IMAGE_LIKE:
      return "image_like";
    case KERNEL_ATTACHMENT_KIND_PDF_LIKE:
      return "pdf_like";
    case KERNEL_ATTACHMENT_KIND_CHEM_LIKE:
      return "chem_like";
    case KERNEL_ATTACHMENT_KIND_UNKNOWN:
    default:
      return "unknown";
  }
}

std::string_view attachment_presence_token(const bool is_missing) {
  return is_missing ? "missing" : "present";
}

std::string_view pdf_metadata_state_token(const kernel::storage::PdfMetadataState state) {
  switch (state) {
    case kernel::storage::PdfMetadataState::Ready:
      return "ready";
    case kernel::storage::PdfMetadataState::Partial:
      return "partial";
    case kernel::storage::PdfMetadataState::Invalid:
      return "invalid";
    case kernel::storage::PdfMetadataState::Unavailable:
    default:
      return "unavailable";
  }
}

std::string_view pdf_doc_title_state_token(const kernel::storage::PdfDocTitleState state) {
  switch (state) {
    case kernel::storage::PdfDocTitleState::Absent:
      return "absent";
    case kernel::storage::PdfDocTitleState::Available:
      return "available";
    case kernel::storage::PdfDocTitleState::Unavailable:
    default:
      return "unavailable";
  }
}

std::string_view pdf_text_layer_state_token(const kernel::storage::PdfTextLayerState state) {
  switch (state) {
    case kernel::storage::PdfTextLayerState::Absent:
      return "absent";
    case kernel::storage::PdfTextLayerState::Present:
      return "present";
    case kernel::storage::PdfTextLayerState::Unavailable:
    default:
      return "unavailable";
  }
}

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

}  // namespace

namespace kernel::core::domain_query {

kernel_status query_attachment_domain_metadata(
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

  out_entries.clear();
  const kernel_attachment_kind kind =
      kernel::core::attachment_path_shape::describe_attachment_path(record.rel_path).kind;
  for (const auto& key : kRegisteredDomainKeys) {
    if (key.carrier_kind != KERNEL_DOMAIN_CARRIER_ATTACHMENT) {
      continue;
    }

    kernel::core::domain_api::DomainMetadataView entry;
    entry.carrier_kind = KERNEL_DOMAIN_CARRIER_ATTACHMENT;
    entry.carrier_key = record.rel_path;
    entry.namespace_name = std::string(key.namespace_name);
    entry.public_schema_revision = key.public_schema_revision;
    entry.key_name = std::string(key.key_name);
    entry.value_kind = key.value_kind;
    entry.flags = KERNEL_DOMAIN_METADATA_FLAG_NONE;

    switch (key.value_source) {
      case DomainValueSource::AttachmentCarrierSurface:
        entry.string_value = "attachment";
        break;
      case DomainValueSource::AttachmentCoarseKind:
        entry.string_value = std::string(attachment_kind_token(kind));
        break;
      case DomainValueSource::AttachmentPresence:
        entry.string_value = std::string(attachment_presence_token(record.is_missing));
        break;
      default:
        continue;
    }

    out_entries.push_back(std::move(entry));
  }

  finalize_entries(limit, out_entries);
  return kernel::core::make_status(KERNEL_OK);
}

kernel_status query_pdf_domain_metadata(
    kernel_handle* handle,
    const char* attachment_rel_path,
    const size_t limit,
    std::vector<kernel::core::domain_api::DomainMetadataView>& out_entries) {
  kernel::storage::PdfMetadataRecord record;
  const kernel_status record_status =
      kernel::core::pdf_query::query_live_pdf_metadata_record(
          handle,
          attachment_rel_path,
          record);
  if (record_status.code != KERNEL_OK) {
    return record_status;
  }

  out_entries.clear();
  for (const auto& key : kRegisteredDomainKeys) {
    if (key.carrier_kind != KERNEL_DOMAIN_CARRIER_PDF) {
      continue;
    }

    kernel::core::domain_api::DomainMetadataView entry;
    entry.carrier_kind = KERNEL_DOMAIN_CARRIER_PDF;
    entry.carrier_key = record.rel_path;
    entry.namespace_name = std::string(key.namespace_name);
    entry.public_schema_revision = key.public_schema_revision;
    entry.key_name = std::string(key.key_name);
    entry.value_kind = key.value_kind;
    entry.flags = KERNEL_DOMAIN_METADATA_FLAG_NONE;

    switch (key.value_source) {
      case DomainValueSource::PdfCarrierSurface:
        entry.string_value = "pdf";
        break;
      case DomainValueSource::PdfPresence:
        entry.string_value = std::string(attachment_presence_token(record.is_missing));
        break;
      case DomainValueSource::PdfMetadataState:
        entry.string_value = std::string(pdf_metadata_state_token(record.metadata_state));
        break;
      case DomainValueSource::PdfDocTitleState:
        entry.string_value = std::string(pdf_doc_title_state_token(record.doc_title_state));
        break;
      case DomainValueSource::PdfTextLayerState:
        entry.string_value = std::string(pdf_text_layer_state_token(record.text_layer_state));
        break;
      case DomainValueSource::PdfPageCount:
        entry.uint64_value = record.page_count;
        break;
      case DomainValueSource::PdfHasOutline:
        entry.bool_value = record.has_outline;
        break;
      default:
        continue;
    }

    out_entries.push_back(std::move(entry));
  }

  finalize_entries(limit, out_entries);
  return kernel::core::make_status(KERNEL_OK);
}

}  // namespace kernel::core::domain_query
