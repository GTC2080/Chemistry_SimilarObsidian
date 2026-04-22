#pragma once

#include "core/kernel_pdf_query_shared.h"
#include "kernel/c_api.h"

#include <string>
#include <string_view>
#include <vector>

std::string make_text_pdf_bytes(std::string_view page_text);

std::string make_metadata_pdf_bytes(
    int page_count,
    bool has_outline,
    bool include_text_layer,
    std::string_view title_clause);

bool try_query_pdf_anchor_records(
    kernel_handle* handle,
    const char* attachment_rel_path,
    std::vector<kernel::storage::PdfAnchorRecord>& out_records);

std::vector<kernel::storage::PdfAnchorRecord> query_pdf_anchor_records(
    kernel_handle* handle,
    const char* attachment_rel_path);
