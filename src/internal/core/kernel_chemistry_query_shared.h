// Reason: This file keeps Track 5 Batch 1 chemistry-metadata query plumbing
// out of the public ABI wrapper so the first chemistry capability slice stays
// thin and focused.

#pragma once

#include "core/kernel_chemistry_api_shared.h"
#include "core/kernel_domain_api_shared.h"
#include "core/kernel_internal.h"

#include <vector>

namespace kernel::core::chemistry_query {

kernel_status query_chem_spectrum_metadata(
    kernel_handle* handle,
    const char* attachment_rel_path,
    size_t limit,
    std::vector<kernel::core::domain_api::DomainMetadataView>& out_entries);
kernel_status query_chem_spectra(
    kernel_handle* handle,
    size_t limit,
    std::vector<kernel::core::chemistry_api::ChemSpectrumView>& out_spectra);
kernel_status query_chem_spectrum(
    kernel_handle* handle,
    const char* attachment_rel_path,
    kernel::core::chemistry_api::ChemSpectrumView& out_spectrum);

}  // namespace kernel::core::chemistry_query
