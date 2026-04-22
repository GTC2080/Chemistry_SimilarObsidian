// Reason: This file keeps Track 5 Batch 3 chemistry-reference query plumbing
// out of the public ABI wrapper so chemistry ref projection stays reusable.

#pragma once

#include "core/kernel_chemistry_reference_api_shared.h"
#include "core/kernel_internal.h"

#include <vector>

namespace kernel::core::chemistry_reference_query {

kernel_status query_note_chem_spectrum_refs(
    kernel_handle* handle,
    const char* note_rel_path,
    size_t limit,
    std::vector<kernel::core::chemistry_reference_api::ChemSpectrumSourceRefView>& out_refs);
kernel_status query_chem_spectrum_referrers(
    kernel_handle* handle,
    const char* attachment_rel_path,
    size_t limit,
    std::vector<kernel::core::chemistry_reference_api::ChemSpectrumReferrerView>& out_referrers);

}  // namespace kernel::core::chemistry_reference_query
