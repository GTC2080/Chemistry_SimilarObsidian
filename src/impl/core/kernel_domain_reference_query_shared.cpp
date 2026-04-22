// Reason: This file owns Track 4 Batch 3 generic domain-reference projection
// so the new substrate grows by reusing the formal PDF reference surface.

#include "core/kernel_domain_reference_query_shared.h"

#include "core/kernel_chemistry_reference_query_shared.h"
#include "core/kernel_domain_object_key.h"
#include "core/kernel_domain_object_query_shared.h"
#include "core/kernel_pdf_reference_query_shared.h"
#include "core/kernel_shared.h"
#include "pdf/pdf_anchor.h"

#include <vector>

namespace {

constexpr std::string_view kGenericNamespace = "generic";
constexpr std::string_view kPdfSubtypeName = "pdf_document";
constexpr std::string_view kChemNamespace = "chem";
constexpr std::string_view kChemSpectrumSubtypeName = "spectrum";

kernel_domain_ref_state map_pdf_ref_state(const kernel_pdf_ref_state state) {
  switch (state) {
    case KERNEL_PDF_REF_RESOLVED:
      return KERNEL_DOMAIN_REF_RESOLVED;
    case KERNEL_PDF_REF_MISSING:
      return KERNEL_DOMAIN_REF_MISSING;
    case KERNEL_PDF_REF_STALE:
      return KERNEL_DOMAIN_REF_STALE;
    case KERNEL_PDF_REF_UNRESOLVED:
    default:
      return KERNEL_DOMAIN_REF_UNRESOLVED;
  }
}

std::string extract_target_basis_revision(std::string_view selector_serialized) {
  kernel::pdf::ParsedPdfAnchor parsed_anchor;
  if (!kernel::pdf::parse_pdf_anchor(selector_serialized, parsed_anchor)) {
    return {};
  }
  return parsed_anchor.pdf_anchor_basis_revision;
}

kernel::core::domain_reference_api::DomainSourceRefView make_domain_source_ref_view(
    const kernel::core::pdf_reference_query::PdfSourceRefView& pdf_ref) {
  kernel::core::domain_reference_api::DomainSourceRefView ref;
  ref.target_object_key = kernel::core::domain_object_key::make_domain_object_key(
      KERNEL_DOMAIN_CARRIER_PDF,
      pdf_ref.pdf_rel_path,
      kGenericNamespace,
      kPdfSubtypeName);
  ref.selector_kind = KERNEL_DOMAIN_SELECTOR_OPAQUE_DOMAIN_SELECTOR;
  ref.selector_serialized = pdf_ref.anchor_serialized;
  ref.preview_text = pdf_ref.excerpt_text;
  ref.target_basis_revision = extract_target_basis_revision(pdf_ref.anchor_serialized);
  ref.state = map_pdf_ref_state(pdf_ref.state);
  ref.flags = KERNEL_DOMAIN_REF_FLAG_NONE;
  return ref;
}

kernel::core::domain_reference_api::DomainReferrerView make_domain_referrer_view(
    const std::string& target_object_key,
    const kernel::core::pdf_reference_query::PdfReferrerView& pdf_referrer) {
  kernel::core::domain_reference_api::DomainReferrerView referrer;
  referrer.note_rel_path = pdf_referrer.note_rel_path;
  referrer.note_title = pdf_referrer.note_title;
  referrer.target_object_key = target_object_key;
  referrer.selector_kind = KERNEL_DOMAIN_SELECTOR_OPAQUE_DOMAIN_SELECTOR;
  referrer.selector_serialized = pdf_referrer.anchor_serialized;
  referrer.preview_text = pdf_referrer.excerpt_text;
  referrer.target_basis_revision = extract_target_basis_revision(pdf_referrer.anchor_serialized);
  referrer.state = map_pdf_ref_state(pdf_referrer.state);
  referrer.flags = KERNEL_DOMAIN_REF_FLAG_NONE;
  return referrer;
}

kernel::core::domain_reference_api::DomainSourceRefView make_chem_domain_source_ref_view(
    const kernel::core::chemistry_reference_api::ChemSpectrumSourceRefView& chem_ref) {
  kernel::core::domain_reference_api::DomainSourceRefView ref;
  ref.target_object_key = chem_ref.domain_object_key;
  ref.selector_kind = KERNEL_DOMAIN_SELECTOR_OPAQUE_DOMAIN_SELECTOR;
  ref.selector_serialized = chem_ref.selector_serialized;
  ref.preview_text = chem_ref.preview_text;
  ref.target_basis_revision = chem_ref.target_basis_revision;
  ref.state = chem_ref.state;
  ref.flags = chem_ref.flags;
  return ref;
}

kernel::core::domain_reference_api::DomainReferrerView make_chem_domain_referrer_view(
    const kernel::core::chemistry_reference_api::ChemSpectrumReferrerView& chem_referrer) {
  kernel::core::domain_reference_api::DomainReferrerView referrer;
  referrer.note_rel_path = chem_referrer.note_rel_path;
  referrer.note_title = chem_referrer.note_title;
  referrer.target_object_key = chem_referrer.domain_object_key;
  referrer.selector_kind = KERNEL_DOMAIN_SELECTOR_OPAQUE_DOMAIN_SELECTOR;
  referrer.selector_serialized = chem_referrer.selector_serialized;
  referrer.preview_text = chem_referrer.preview_text;
  referrer.target_basis_revision = chem_referrer.target_basis_revision;
  referrer.state = chem_referrer.state;
  referrer.flags = chem_referrer.flags;
  return referrer;
}

bool is_pdf_document_object(
    const kernel::core::domain_object_api::DomainObjectView& object) {
  return object.carrier_kind == KERNEL_DOMAIN_CARRIER_PDF &&
         object.subtype_namespace == kGenericNamespace &&
         object.subtype_name == kPdfSubtypeName;
}

bool is_chem_spectrum_object(
    const kernel::core::domain_object_api::DomainObjectView& object) {
  return object.carrier_kind == KERNEL_DOMAIN_CARRIER_ATTACHMENT &&
         object.subtype_namespace == kChemNamespace &&
         object.subtype_name == kChemSpectrumSubtypeName;
}

}  // namespace

namespace kernel::core::domain_reference_query {

kernel_status query_note_domain_source_refs(
    kernel_handle* handle,
    const char* note_rel_path,
    const size_t limit,
    std::vector<kernel::core::domain_reference_api::DomainSourceRefView>& out_refs) {
  std::vector<kernel::core::pdf_reference_query::PdfSourceRefView> pdf_refs;
  const kernel_status query_status =
      kernel::core::pdf_reference_query::query_note_pdf_source_refs(
          handle,
          note_rel_path,
          static_cast<size_t>(-1),
          pdf_refs);
  if (query_status.code != KERNEL_OK) {
    return query_status;
  }

  std::vector<kernel::core::chemistry_reference_api::ChemSpectrumSourceRefView> chem_refs;
  const kernel_status chem_status =
      kernel::core::chemistry_reference_query::query_note_chem_spectrum_refs(
          handle,
          note_rel_path,
          static_cast<size_t>(-1),
          chem_refs);
  if (chem_status.code != KERNEL_OK) {
    return chem_status;
  }

  out_refs.clear();
  out_refs.reserve(pdf_refs.size() + chem_refs.size());
  for (const auto& pdf_ref : pdf_refs) {
    out_refs.push_back(make_domain_source_ref_view(pdf_ref));
  }
  for (const auto& chem_ref : chem_refs) {
    out_refs.push_back(make_chem_domain_source_ref_view(chem_ref));
  }
  if (out_refs.size() > limit) {
    out_refs.resize(limit);
  }
  return kernel::core::make_status(KERNEL_OK);
}

kernel_status query_domain_object_referrers(
    kernel_handle* handle,
    const char* domain_object_key,
    const size_t limit,
    std::vector<kernel::core::domain_reference_api::DomainReferrerView>& out_referrers) {
  kernel::core::domain_object_api::DomainObjectView object;
  const kernel_status object_status =
      kernel::core::domain_object_query::query_domain_object(handle, domain_object_key, object);
  if (object_status.code != KERNEL_OK) {
    return object_status;
  }

  out_referrers.clear();
  if (!is_pdf_document_object(object)) {
    if (!is_chem_spectrum_object(object)) {
      return kernel::core::make_status(KERNEL_OK);
    }

    std::vector<kernel::core::chemistry_reference_api::ChemSpectrumReferrerView> chem_referrers;
    const kernel_status chem_status =
        kernel::core::chemistry_reference_query::query_chem_spectrum_referrers(
            handle,
            object.carrier_key.c_str(),
            limit,
            chem_referrers);
    if (chem_status.code != KERNEL_OK) {
      return chem_status;
    }

    out_referrers.reserve(chem_referrers.size());
    for (const auto& chem_referrer : chem_referrers) {
      out_referrers.push_back(make_chem_domain_referrer_view(chem_referrer));
    }
    return kernel::core::make_status(KERNEL_OK);
  }

  std::vector<kernel::core::pdf_reference_query::PdfReferrerView> pdf_referrers;
  const kernel_status referrer_status =
      kernel::core::pdf_reference_query::query_pdf_referrers(
          handle,
          object.carrier_key.c_str(),
          limit,
          pdf_referrers);
  if (referrer_status.code != KERNEL_OK) {
    return referrer_status;
  }

  out_referrers.reserve(pdf_referrers.size());
  for (const auto& pdf_referrer : pdf_referrers) {
    out_referrers.push_back(make_domain_referrer_view(object.domain_object_key, pdf_referrer));
  }
  return kernel::core::make_status(KERNEL_OK);
}

}  // namespace kernel::core::domain_reference_query
