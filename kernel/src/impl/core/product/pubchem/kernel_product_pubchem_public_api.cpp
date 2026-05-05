// Reason: Expose PubChem product rules through focused C ABI wrappers.

#include "kernel/c_api.h"

#include "core/kernel_product_pubchem.h"
#include "core/kernel_product_public_api_utils.h"
#include "core/kernel_shared.h"

#include <cstdint>
#include <string>
#include <string_view>

extern "C" kernel_status kernel_normalize_pubchem_query(
    const char* query,
    const std::size_t query_size,
    kernel_owned_buffer* out_buffer) {
  if (out_buffer == nullptr || (query_size > 0 && query == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  const std::string_view raw(query == nullptr ? "" : query, query_size);
  const std::string normalized = kernel::core::product::normalize_pubchem_query(raw);
  if (normalized.empty()) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  if (!kernel::core::product::api::fill_owned_buffer(normalized, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_build_pubchem_compound_info_json(
    const char* query,
    const std::size_t query_size,
    const char* formula,
    const std::size_t formula_size,
    const double molecular_weight,
    const std::uint8_t has_density,
    const double density,
    const std::size_t property_count,
    kernel_owned_buffer* out_buffer) {
  if (
      out_buffer == nullptr || (query_size > 0 && query == nullptr) ||
      (formula_size > 0 && formula == nullptr)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  out_buffer->data = nullptr;
  out_buffer->size = 0;

  const std::string_view query_view(query == nullptr ? "" : query, query_size);
  const std::string_view formula_view(formula == nullptr ? "" : formula, formula_size);
  const std::string payload = kernel::core::product::build_pubchem_compound_info_payload_json(
      query_view,
      formula_view,
      molecular_weight,
      has_density != 0,
      density,
      property_count);
  if (!kernel::core::product::api::fill_owned_buffer(payload, out_buffer)) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }
  return kernel::core::make_status(KERNEL_OK);
}
