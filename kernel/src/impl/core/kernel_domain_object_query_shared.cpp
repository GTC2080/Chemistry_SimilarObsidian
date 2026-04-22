// Reason: This file owns Track 4 Batch 2 derived subtype projection so the
// formal object surface stays independent from metadata and future refs.

#include "core/kernel_domain_object_query_shared.h"

#include "core/kernel_attachment_path_shape.h"
#include "core/kernel_attachment_query_shared.h"
#include "core/kernel_chemistry_query_shared.h"
#include "core/kernel_domain_object_key.h"
#include "core/kernel_pdf_query_shared.h"
#include "core/kernel_shared.h"

#include <algorithm>
#include <vector>

namespace {

constexpr std::string_view kGenericNamespace = "generic";
constexpr std::string_view kAttachmentSubtypeName = "attachment_resource";
constexpr std::string_view kPdfSubtypeName = "pdf_document";

void finalize_domain_objects(
    const size_t limit,
    std::vector<kernel::core::domain_object_api::DomainObjectView>& out_objects) {
  std::sort(
      out_objects.begin(),
      out_objects.end(),
      [](const auto& lhs, const auto& rhs) {
        if (lhs.subtype_namespace != rhs.subtype_namespace) {
          return lhs.subtype_namespace < rhs.subtype_namespace;
        }
        return lhs.subtype_name < rhs.subtype_name;
      });
  if (out_objects.size() > limit) {
    out_objects.resize(limit);
  }
}

kernel::core::domain_object_api::DomainObjectView make_attachment_object_view(
    const kernel::storage::AttachmentCatalogRecord& record) {
  kernel::core::domain_object_api::DomainObjectView object;
  object.carrier_kind = KERNEL_DOMAIN_CARRIER_ATTACHMENT;
  object.carrier_key = record.rel_path;
  object.subtype_namespace = std::string(kGenericNamespace);
  object.subtype_name = std::string(kAttachmentSubtypeName);
  object.subtype_revision = kernel::core::domain_object_api::kGenericAttachmentSubtypeRevision;
  object.coarse_kind =
      kernel::core::attachment_path_shape::describe_attachment_path(record.rel_path).kind;
  object.presence = record.is_missing ? KERNEL_ATTACHMENT_PRESENCE_MISSING
                                      : KERNEL_ATTACHMENT_PRESENCE_PRESENT;
  object.state = record.is_missing ? KERNEL_DOMAIN_OBJECT_MISSING
                                   : KERNEL_DOMAIN_OBJECT_PRESENT;
  object.flags = KERNEL_DOMAIN_OBJECT_FLAG_NONE;
  object.domain_object_key = kernel::core::domain_object_key::make_domain_object_key(
      object.carrier_kind,
      object.carrier_key,
      object.subtype_namespace,
      object.subtype_name);
  return object;
}

kernel_domain_object_state pdf_domain_object_state(
    const kernel::storage::PdfMetadataRecord& record) {
  if (record.is_missing) {
    return KERNEL_DOMAIN_OBJECT_MISSING;
  }
  switch (record.metadata_state) {
    case kernel::storage::PdfMetadataState::Ready:
    case kernel::storage::PdfMetadataState::Partial:
      return KERNEL_DOMAIN_OBJECT_PRESENT;
    case kernel::storage::PdfMetadataState::Invalid:
    case kernel::storage::PdfMetadataState::Unavailable:
    default:
      return KERNEL_DOMAIN_OBJECT_UNRESOLVED;
  }
}

kernel::core::domain_object_api::DomainObjectView make_pdf_object_view(
    const kernel::storage::PdfMetadataRecord& record) {
  kernel::core::domain_object_api::DomainObjectView object;
  object.carrier_kind = KERNEL_DOMAIN_CARRIER_PDF;
  object.carrier_key = record.rel_path;
  object.subtype_namespace = std::string(kGenericNamespace);
  object.subtype_name = std::string(kPdfSubtypeName);
  object.subtype_revision = kernel::core::domain_object_api::kGenericPdfSubtypeRevision;
  object.coarse_kind = KERNEL_ATTACHMENT_KIND_PDF_LIKE;
  object.presence = record.is_missing ? KERNEL_ATTACHMENT_PRESENCE_MISSING
                                      : KERNEL_ATTACHMENT_PRESENCE_PRESENT;
  object.state = pdf_domain_object_state(record);
  object.flags = KERNEL_DOMAIN_OBJECT_FLAG_NONE;
  object.domain_object_key = kernel::core::domain_object_key::make_domain_object_key(
      object.carrier_kind,
      object.carrier_key,
      object.subtype_namespace,
      object.subtype_name);
  return object;
}

kernel::core::domain_object_api::DomainObjectView make_chem_spectrum_object_view(
    const kernel::core::chemistry_api::ChemSpectrumView& spectrum) {
  kernel::core::domain_object_api::DomainObjectView object;
  object.domain_object_key = spectrum.domain_object_key;
  object.carrier_kind = KERNEL_DOMAIN_CARRIER_ATTACHMENT;
  object.carrier_key = spectrum.attachment_rel_path;
  object.subtype_namespace = "chem";
  object.subtype_name = "spectrum";
  object.subtype_revision = spectrum.subtype_revision;
  object.coarse_kind = spectrum.coarse_kind;
  object.presence = spectrum.presence;
  object.state = spectrum.state;
  object.flags = spectrum.flags;
  return object;
}

}  // namespace

namespace kernel::core::domain_object_query {

kernel_status query_attachment_domain_objects(
    kernel_handle* handle,
    const char* attachment_rel_path,
    const size_t limit,
    std::vector<kernel::core::domain_object_api::DomainObjectView>& out_objects) {
  kernel::storage::AttachmentCatalogRecord record;
  const kernel_status record_status =
      kernel::core::attachment_query::query_live_attachment_record(
          handle,
          attachment_rel_path,
          record);
  if (record_status.code != KERNEL_OK) {
    return record_status;
  }

  out_objects.clear();
  out_objects.push_back(make_attachment_object_view(record));
  finalize_domain_objects(limit, out_objects);
  return kernel::core::make_status(KERNEL_OK);
}

kernel_status query_pdf_domain_objects(
    kernel_handle* handle,
    const char* attachment_rel_path,
    const size_t limit,
    std::vector<kernel::core::domain_object_api::DomainObjectView>& out_objects) {
  kernel::storage::PdfMetadataRecord record;
  const kernel_status record_status =
      kernel::core::pdf_query::query_live_pdf_metadata_record(
          handle,
          attachment_rel_path,
          record);
  if (record_status.code != KERNEL_OK) {
    return record_status;
  }

  out_objects.clear();
  out_objects.push_back(make_pdf_object_view(record));
  finalize_domain_objects(limit, out_objects);
  return kernel::core::make_status(KERNEL_OK);
}

kernel_status query_domain_object(
    kernel_handle* handle,
    const char* domain_object_key,
    kernel::core::domain_object_api::DomainObjectView& out_object) {
  if (kernel::core::is_null_or_empty(domain_object_key)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel::core::domain_object_key::ParsedDomainObjectKey parsed_key;
  if (!kernel::core::domain_object_key::parse_domain_object_key(
          domain_object_key,
          parsed_key)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string canonical_key = kernel::core::domain_object_key::make_domain_object_key(
      parsed_key.carrier_kind,
      parsed_key.carrier_key,
      parsed_key.subtype_namespace,
      parsed_key.subtype_name);
  if (canonical_key != domain_object_key) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::vector<kernel::core::domain_object_api::DomainObjectView> objects;
  kernel_status query_status = kernel::core::make_status(KERNEL_ERROR_NOT_FOUND);
  if (parsed_key.carrier_kind == KERNEL_DOMAIN_CARRIER_ATTACHMENT &&
      parsed_key.subtype_namespace == "chem" &&
      parsed_key.subtype_name == "spectrum") {
    kernel::core::chemistry_api::ChemSpectrumView spectrum;
    query_status = kernel::core::chemistry_query::query_chem_spectrum(
        handle,
        parsed_key.carrier_key.c_str(),
        spectrum);
    if (query_status.code != KERNEL_OK) {
      return query_status;
    }
    out_object = make_chem_spectrum_object_view(spectrum);
    if (out_object.domain_object_key == domain_object_key) {
      return kernel::core::make_status(KERNEL_OK);
    }
    return kernel::core::make_status(KERNEL_ERROR_NOT_FOUND);
  }

  switch (parsed_key.carrier_kind) {
    case KERNEL_DOMAIN_CARRIER_ATTACHMENT:
      query_status = query_attachment_domain_objects(
          handle,
          parsed_key.carrier_key.c_str(),
          static_cast<size_t>(-1),
          objects);
      break;
    case KERNEL_DOMAIN_CARRIER_PDF:
      query_status = query_pdf_domain_objects(
          handle,
          parsed_key.carrier_key.c_str(),
          static_cast<size_t>(-1),
          objects);
      break;
  }
  if (query_status.code != KERNEL_OK) {
    return query_status;
  }

  for (const auto& object : objects) {
    if (object.domain_object_key == domain_object_key) {
      out_object = object;
      return kernel::core::make_status(KERNEL_OK);
    }
  }

  return kernel::core::make_status(KERNEL_ERROR_NOT_FOUND);
}

}  // namespace kernel::core::domain_object_query
