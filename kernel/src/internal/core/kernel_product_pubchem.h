// Reason: Keep PubChem payload normalization out of the product public ABI wrapper.

#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace kernel::core::product {

std::string normalize_pubchem_query(std::string_view query);

std::string build_pubchem_compound_info_payload_json(
    std::string_view query,
    std::string_view formula,
    double molecular_weight,
    bool has_density,
    double density,
    std::size_t property_count);

}  // namespace kernel::core::product
