// Reason: This file records repeatable host-facing query timing loops for search, attachment, tag, and backlink surfaces.

#include "kernel/c_api.h"
#include "benchmarks/benchmark_thresholds.h"
#include "core/kernel_pdf_query_shared.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool expect_ok(const kernel_status status, const char* operation) {
  if (status.code == KERNEL_OK) {
    return true;
  }
  std::cerr << operation << " failed with code " << status.code << "\n";
  return false;
}

bool seed_note(
    kernel_handle* handle,
    const char* rel_path,
    const std::string& text,
    kernel_note_metadata& metadata,
    kernel_write_disposition& disposition) {
  return expect_ok(
      kernel_write_note(
          handle,
          rel_path,
          text.data(),
          text.size(),
          nullptr,
          &metadata,
          &disposition),
      rel_path);
}

bool seed_binary_file(const std::filesystem::path& path, const std::string& bytes) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream) {
    std::cerr << "failed to create file: " << path << "\n";
    return false;
  }
  stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  return stream.good();
}

kernel_search_query make_default_query(const char* query, const std::size_t limit) {
  kernel_search_query request{};
  request.query = query;
  request.limit = limit;
  request.offset = 0;
  request.kind = KERNEL_SEARCH_KIND_NOTE;
  request.tag_filter = nullptr;
  request.path_prefix = nullptr;
  request.include_deleted = 0;
  request.sort_mode = KERNEL_SEARCH_SORT_REL_PATH_ASC;
  return request;
}

std::string make_pdf_bytes(const std::vector<std::string>& page_texts) {
  std::string bytes = "%PDF-1.7\n1 0 obj\n<< /Type /Catalog >>\nendobj\n";
  for (std::size_t page = 0; page < page_texts.size(); ++page) {
    bytes += std::to_string(page + 2);
    bytes += " 0 obj\n<< /Type /Page >>\n";
    if (!page_texts[page].empty()) {
      bytes += "BT\n/F1 12 Tf\n(";
      bytes += page_texts[page];
      bytes += ") Tj\nET\n";
    }
    bytes += "endobj\n";
  }
  bytes += "%%EOF\n";
  return bytes;
}

bool query_pdf_anchors(
    kernel_handle* handle,
    const char* attachment_rel_path,
    std::vector<kernel::storage::PdfAnchorRecord>& out_records) {
  return expect_ok(
      kernel::core::pdf_query::query_live_pdf_anchor_records(
          handle,
          attachment_rel_path,
          out_records),
      "pdf anchor query");
}

}  // namespace

int main() {
  const auto vault = std::filesystem::temp_directory_path() / "chem_kernel_query_bench";
  std::filesystem::remove_all(vault);
  std::filesystem::create_directories(vault);

  kernel_handle* handle = nullptr;
  if (!expect_ok(kernel_open_vault(vault.string().c_str(), &handle), "open")) {
    return 1;
  }

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  if (!seed_note(handle, "target.md", "# Shared Target\nbody\n", metadata, disposition)) {
    return 1;
  }
  if (!seed_note(
          handle,
          "search-target.md",
          "# QueryTitleToken\nbody contains QueryBodyToken\n",
          metadata,
          disposition)) {
    return 1;
  }
  if (!seed_note(
          handle,
          "title-only-target.md",
          "# QueryTitleOnlyToken\nbody text without the unique title token\n",
          metadata,
          disposition)) {
    return 1;
  }
  if (!seed_note(
          handle,
          "a-rank-body.md",
          "# Generic Rank Title\nBenchmarkRankToken appears only in the body\n",
          metadata,
          disposition)) {
    return 1;
  }
  if (!seed_note(
          handle,
          "z-rank-title.md",
          "# BenchmarkRankToken\nbody text without the unique rank token\n",
          metadata,
          disposition)) {
    return 1;
  }
  if (!seed_note(
          handle,
          "a-rank-untagged.md",
          "# Rank Boost Untagged\nrankboost benchmark body token\n",
          metadata,
          disposition)) {
    return 1;
  }
  if (!seed_note(
          handle,
          "z-rank-tagged.md",
          "# Rank Boost Tagged\n#rankboost\nrankboost benchmark body token\n",
          metadata,
          disposition)) {
    return 1;
  }

  constexpr int note_count = 64;
  for (int i = 0; i < note_count; ++i) {
    const std::string rel_path = "source-" + std::to_string(i) + ".md";
    const std::string body =
        "# Source " + std::to_string(i) + "\n#benchtag\n[[Shared Target]]\nPageBodyToken\n";
    if (!seed_note(handle, rel_path.c_str(), body, metadata, disposition)) {
      return 1;
    }
  }

  constexpr int filter_note_count = 16;
  for (int i = 0; i < filter_note_count; ++i) {
    const std::string suffix = (i < 10 ? "0" : "") + std::to_string(i);
    const std::string attachment_rel_path = "filter/filtermixedtoken-" + suffix + ".png";
    if (!seed_binary_file(vault / attachment_rel_path, "filter-bytes-" + suffix)) {
      return 1;
    }

    const std::string rel_path = "filter/filter-note-" + suffix + ".md";
    const std::string title =
        i == filter_note_count - 1
            ? "# FilterMixedToken\n"
            : "# Filter " + suffix + "\n";
    const std::string body =
        title +
        "#filtertag\n"
        "FilterMixedToken body " + suffix + "\n"
        "![Figure](" + attachment_rel_path + ")\n";
    if (!seed_note(handle, rel_path.c_str(), body, metadata, disposition)) {
      return 1;
    }
  }

  if (!seed_binary_file(vault / "pdf" / "bench-source.pdf", make_pdf_bytes({"Benchmark PDF Source Text"}))) {
    return 1;
  }
  if (!seed_note(
          handle,
          "pdf/bench-seed.md",
          "# PDF Bench Seed\n[PDF](pdf/bench-source.pdf)\n",
          metadata,
          disposition)) {
    return 1;
  }

  std::vector<kernel::storage::PdfAnchorRecord> pdf_anchor_records;
  if (!query_pdf_anchors(handle, "pdf/bench-source.pdf", pdf_anchor_records) ||
      pdf_anchor_records.size() != 1) {
    std::cerr << "pdf anchor query returned unexpected anchor count\n";
    return 1;
  }
  const std::string pdf_anchor = pdf_anchor_records[0].anchor_serialized;

  if (!seed_note(
          handle,
          "pdf/bench-source-refs.md",
          "# PDF Bench Source Refs\n"
          "[First](pdf/bench-source.pdf#anchor=" + pdf_anchor + ")\n"
          "[Second](pdf/bench-source.pdf#anchor=" + pdf_anchor + ")\n",
          metadata,
          disposition)) {
    return 1;
  }
  if (!seed_note(
          handle,
          "pdf/bench-referrer.md",
          "# PDF Bench Referrer\n"
          "[Third](pdf/bench-source.pdf#anchor=" + pdf_anchor + ")\n",
          metadata,
          disposition)) {
    return 1;
  }

  constexpr int iterations = 200;
  constexpr const char* kBenchAttachmentRelPath = "filter/filtermixedtoken-00.png";
  constexpr const char* kBenchAttachmentNoteRelPath = "filter/filter-note-00.md";
  constexpr const char* kBenchPdfRelPath = "pdf/bench-source.pdf";
  constexpr const char* kBenchPdfSourceNoteRelPath = "pdf/bench-source-refs.md";

  const auto tag_start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i) {
    kernel_search_results results{};
    if (!expect_ok(kernel_query_tag_notes(handle, "benchtag", static_cast<size_t>(-1), &results), "tag query")) {
      return 1;
    }
    if (results.count != note_count) {
      std::cerr << "tag query returned unexpected hit count\n";
      return 1;
    }
    kernel_free_search_results(&results);
  }
  const auto tag_end = std::chrono::steady_clock::now();

  const auto title_search_start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i) {
    kernel_search_results results{};
    if (!expect_ok(kernel_search_notes_limited(handle, "QueryTitleToken", 1, &results), "title search")) {
      return 1;
    }
    if (results.count != 1) {
      std::cerr << "title search returned unexpected hit count\n";
      return 1;
    }
    kernel_free_search_results(&results);
  }
  const auto title_search_end = std::chrono::steady_clock::now();

  const auto body_search_start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i) {
    kernel_search_results results{};
    if (!expect_ok(kernel_search_notes_limited(handle, "QueryBodyToken", 1, &results), "body search")) {
      return 1;
    }
    if (results.count != 1) {
      std::cerr << "body search returned unexpected hit count\n";
      return 1;
    }
    kernel_free_search_results(&results);
  }
  const auto body_search_end = std::chrono::steady_clock::now();

  const auto body_snippet_start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i) {
    kernel_search_query request = make_default_query("QueryBodyToken", 1);
    kernel_search_page page{};
    if (!expect_ok(kernel_query_search(handle, &request, &page), "body snippet search")) {
      return 1;
    }
    if (page.count != 1 || page.total_hits != 1 ||
        page.hits[0].snippet_status != KERNEL_SEARCH_SNIPPET_BODY_EXTRACTED) {
      std::cerr << "body snippet search returned unexpected page state\n";
      return 1;
    }
    kernel_free_search_page(&page);
  }
  const auto body_snippet_end = std::chrono::steady_clock::now();

  const auto title_only_search_start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i) {
    kernel_search_query request = make_default_query("QueryTitleOnlyToken", 1);
    kernel_search_page page{};
    if (!expect_ok(kernel_query_search(handle, &request, &page), "title-only expanded search")) {
      return 1;
    }
    if (page.count != 1 || page.total_hits != 1 ||
        page.hits[0].snippet_status != KERNEL_SEARCH_SNIPPET_TITLE_ONLY) {
      std::cerr << "title-only expanded search returned unexpected page state\n";
      return 1;
    }
    kernel_free_search_page(&page);
  }
  const auto title_only_search_end = std::chrono::steady_clock::now();

  const auto shallow_page_start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i) {
    kernel_search_query request = make_default_query("PageBodyToken", 10);
    kernel_search_page page{};
    if (!expect_ok(kernel_query_search(handle, &request, &page), "shallow page search")) {
      return 1;
    }
    if (page.count != 10 || page.total_hits != note_count || page.has_more == 0) {
      std::cerr << "shallow page search returned unexpected page state\n";
      return 1;
    }
    kernel_free_search_page(&page);
  }
  const auto shallow_page_end = std::chrono::steady_clock::now();

  const auto deep_offset_start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i) {
    kernel_search_query request = make_default_query("PageBodyToken", 8);
    request.offset = 48;
    kernel_search_page page{};
    if (!expect_ok(kernel_query_search(handle, &request, &page), "deep offset search")) {
      return 1;
    }
    if (page.count != 8 || page.total_hits != note_count || page.has_more == 0) {
      std::cerr << "deep offset search returned unexpected page state\n";
      return 1;
    }
    kernel_free_search_page(&page);
  }
  const auto deep_offset_end = std::chrono::steady_clock::now();

  const auto filtered_note_start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i) {
    kernel_search_query request = make_default_query("FilterMixedToken", 8);
    request.tag_filter = "filtertag";
    request.path_prefix = "filter/";
    kernel_search_page page{};
    if (!expect_ok(kernel_query_search(handle, &request, &page), "filtered note query")) {
      return 1;
    }
    if (page.count != 8 || page.total_hits != filter_note_count || page.has_more == 0) {
      std::cerr << "filtered note query returned unexpected page state\n";
      return 1;
    }
    kernel_free_search_page(&page);
  }
  const auto filtered_note_end = std::chrono::steady_clock::now();

  const auto attachment_path_start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i) {
    kernel_search_query request = make_default_query("filtermixedtoken", 8);
    request.kind = KERNEL_SEARCH_KIND_ATTACHMENT;
    request.path_prefix = "filter/";
    kernel_search_page page{};
    if (!expect_ok(kernel_query_search(handle, &request, &page), "attachment path query")) {
      return 1;
    }
    if (page.count != 8 || page.total_hits != filter_note_count || page.has_more == 0) {
      std::cerr << "attachment path query returned unexpected page state\n";
      return 1;
    }
    kernel_free_search_page(&page);
  }
  const auto attachment_path_end = std::chrono::steady_clock::now();

  const auto attachment_catalog_start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i) {
    kernel_attachment_list attachments{};
    if (!expect_ok(kernel_query_attachments(handle, 8, &attachments), "attachment catalog query")) {
      return 1;
    }
    if (attachments.count != 8 ||
        std::string(attachments.attachments[0].rel_path) != kBenchAttachmentRelPath ||
        attachments.attachments[0].presence != KERNEL_ATTACHMENT_PRESENCE_PRESENT ||
        attachments.attachments[0].ref_count != 1) {
      std::cerr << "attachment catalog query returned unexpected attachment state\n";
      return 1;
    }
    kernel_free_attachment_list(&attachments);
  }
  const auto attachment_catalog_end = std::chrono::steady_clock::now();

  const auto attachment_lookup_start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i) {
    kernel_attachment_record attachment{};
    if (!expect_ok(kernel_get_attachment(handle, kBenchAttachmentRelPath, &attachment), "attachment lookup query")) {
      return 1;
    }
    if (std::string(attachment.rel_path) != kBenchAttachmentRelPath ||
        std::string(attachment.basename) != "filtermixedtoken-00.png" ||
        attachment.presence != KERNEL_ATTACHMENT_PRESENCE_PRESENT ||
        attachment.kind != KERNEL_ATTACHMENT_KIND_IMAGE_LIKE ||
        attachment.ref_count != 1) {
      std::cerr << "attachment lookup query returned unexpected attachment state\n";
      return 1;
    }
    kernel_free_attachment_record(&attachment);
  }
  const auto attachment_lookup_end = std::chrono::steady_clock::now();

  const auto note_attachment_refs_start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i) {
    kernel_attachment_list refs{};
    if (!expect_ok(
            kernel_query_note_attachment_refs(handle, kBenchAttachmentNoteRelPath, 4, &refs),
            "note attachment refs query")) {
      return 1;
    }
    if (refs.count != 1 || std::string(refs.attachments[0].rel_path) != kBenchAttachmentRelPath) {
      std::cerr << "note attachment refs query returned unexpected attachment state\n";
      return 1;
    }
    kernel_free_attachment_list(&refs);
  }
  const auto note_attachment_refs_end = std::chrono::steady_clock::now();

  const auto attachment_referrers_start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i) {
    kernel_attachment_referrers referrers{};
    if (!expect_ok(
            kernel_query_attachment_referrers(handle, kBenchAttachmentRelPath, 4, &referrers),
            "attachment referrers query")) {
      return 1;
    }
    if (referrers.count != 1 ||
        std::string(referrers.referrers[0].note_rel_path) != kBenchAttachmentNoteRelPath) {
      std::cerr << "attachment referrers query returned unexpected referrer state\n";
      return 1;
    }
    kernel_free_attachment_referrers(&referrers);
  }
  const auto attachment_referrers_end = std::chrono::steady_clock::now();

  const auto pdf_source_refs_start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i) {
    kernel_pdf_source_refs refs{};
    if (!expect_ok(
            kernel_query_note_pdf_source_refs(handle, kBenchPdfSourceNoteRelPath, 4, &refs),
            "pdf source refs query")) {
      return 1;
    }
    if (refs.count != 2 ||
        std::string(refs.refs[0].pdf_rel_path) != kBenchPdfRelPath ||
        refs.refs[0].state != KERNEL_PDF_REF_RESOLVED) {
      std::cerr << "pdf source refs query returned unexpected source-ref state\n";
      return 1;
    }
    kernel_free_pdf_source_refs(&refs);
  }
  const auto pdf_source_refs_end = std::chrono::steady_clock::now();

  const auto pdf_referrers_start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i) {
    kernel_pdf_referrers referrers{};
    if (!expect_ok(
            kernel_query_pdf_referrers(handle, kBenchPdfRelPath, 4, &referrers),
            "pdf referrers query")) {
      return 1;
    }
    if (referrers.count != 3 ||
        std::string(referrers.referrers[0].note_rel_path) != "pdf/bench-referrer.md" ||
        referrers.referrers[0].state != KERNEL_PDF_REF_RESOLVED) {
      std::cerr << "pdf referrers query returned unexpected referrer state\n";
      return 1;
    }
    kernel_free_pdf_referrers(&referrers);
  }
  const auto pdf_referrers_end = std::chrono::steady_clock::now();

  const auto all_kind_start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i) {
    kernel_search_query request = make_default_query("FilterMixedToken", 12);
    request.kind = KERNEL_SEARCH_KIND_ALL;
    request.tag_filter = "filtertag";
    request.path_prefix = "filter/";
    kernel_search_page page{};
    if (!expect_ok(kernel_query_search(handle, &request, &page), "all-kind query")) {
      return 1;
    }
    if (page.count != 12 || page.total_hits != filter_note_count * 2 || page.has_more == 0) {
      std::cerr << "all-kind query returned unexpected page state\n";
      return 1;
    }
    kernel_free_search_page(&page);
  }
  const auto all_kind_end = std::chrono::steady_clock::now();

  const auto ranked_title_start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i) {
    kernel_search_query request = make_default_query("BenchmarkRankToken", 2);
    request.sort_mode = KERNEL_SEARCH_SORT_RANK_V1;
    kernel_search_page page{};
    if (!expect_ok(kernel_query_search(handle, &request, &page), "ranked title query")) {
      return 1;
    }
    if (page.count != 2 || page.total_hits != 2 ||
        std::string(page.hits[0].rel_path) != "z-rank-title.md") {
      std::cerr << "ranked title query returned unexpected page state\n";
      return 1;
    }
    kernel_free_search_page(&page);
  }
  const auto ranked_title_end = std::chrono::steady_clock::now();

  const auto ranked_tag_boost_start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i) {
    kernel_search_query request = make_default_query("rankboost", 2);
    request.sort_mode = KERNEL_SEARCH_SORT_RANK_V1;
    kernel_search_page page{};
    if (!expect_ok(kernel_query_search(handle, &request, &page), "ranked tag boost query")) {
      return 1;
    }
    if (page.count != 2 || page.total_hits != 2 ||
        std::string(page.hits[0].rel_path) != "z-rank-tagged.md") {
      std::cerr << "ranked tag boost query returned unexpected page state\n";
      return 1;
    }
    kernel_free_search_page(&page);
  }
  const auto ranked_tag_boost_end = std::chrono::steady_clock::now();

  const auto ranked_all_kind_start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i) {
    kernel_search_query request = make_default_query("FilterMixedToken", 2);
    request.kind = KERNEL_SEARCH_KIND_ALL;
    request.tag_filter = "filtertag";
    request.path_prefix = "filter/";
    request.offset = filter_note_count - 1;
    request.sort_mode = KERNEL_SEARCH_SORT_RANK_V1;
    kernel_search_page page{};
    if (!expect_ok(kernel_query_search(handle, &request, &page), "ranked all-kind query")) {
      return 1;
    }
    if (page.count != 2 || page.total_hits != filter_note_count * 2 ||
        std::string(page.hits[0].rel_path) != "filter/filter-note-14.md" ||
        std::string(page.hits[1].rel_path) != "filter/filtermixedtoken-00.png") {
      std::cerr << "ranked all-kind query returned unexpected page state\n";
      return 1;
    }
    kernel_free_search_page(&page);
  }
  const auto ranked_all_kind_end = std::chrono::steady_clock::now();

  const auto backlink_start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i) {
    kernel_search_results results{};
    if (!expect_ok(kernel_query_backlinks(handle, "target.md", static_cast<size_t>(-1), &results), "backlinks query")) {
      return 1;
    }
    if (results.count != note_count) {
      std::cerr << "backlinks query returned unexpected hit count\n";
      return 1;
    }
    kernel_free_search_results(&results);
  }
  const auto backlink_end = std::chrono::steady_clock::now();

  const auto tag_elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(tag_end - tag_start).count();
  const auto title_search_elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(title_search_end - title_search_start).count();
  const auto body_search_elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(body_search_end - body_search_start).count();
  const auto body_snippet_elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(body_snippet_end - body_snippet_start).count();
  const auto title_only_search_elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(title_only_search_end - title_only_search_start).count();
  const auto shallow_page_elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(shallow_page_end - shallow_page_start).count();
  const auto deep_offset_elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(deep_offset_end - deep_offset_start).count();
  const auto filtered_note_elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(filtered_note_end - filtered_note_start).count();
  const auto attachment_path_elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(attachment_path_end - attachment_path_start).count();
  const auto attachment_catalog_elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(attachment_catalog_end - attachment_catalog_start).count();
  const auto attachment_lookup_elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(attachment_lookup_end - attachment_lookup_start).count();
  const auto note_attachment_refs_elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(note_attachment_refs_end - note_attachment_refs_start)
          .count();
  const auto attachment_referrers_elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(attachment_referrers_end - attachment_referrers_start)
          .count();
  const auto pdf_source_refs_elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(pdf_source_refs_end - pdf_source_refs_start)
          .count();
  const auto pdf_referrers_elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(pdf_referrers_end - pdf_referrers_start)
          .count();
  const auto all_kind_elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(all_kind_end - all_kind_start).count();
  const auto ranked_title_elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(ranked_title_end - ranked_title_start).count();
  const auto ranked_tag_boost_elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(ranked_tag_boost_end - ranked_tag_boost_start).count();
  const auto ranked_all_kind_elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(ranked_all_kind_end - ranked_all_kind_start).count();
  const auto backlink_elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(backlink_end - backlink_start).count();

  const bool tag_within_gate =
      kernel::benchmarks::report_gate(kernel::benchmarks::kTagQueryGate, tag_elapsed_ms);
  const bool title_within_gate =
      kernel::benchmarks::report_gate(kernel::benchmarks::kTitleQueryGate, title_search_elapsed_ms);
  const bool body_within_gate =
      kernel::benchmarks::report_gate(kernel::benchmarks::kBodyQueryGate, body_search_elapsed_ms);
  const bool body_snippet_within_gate =
      kernel::benchmarks::report_gate(kernel::benchmarks::kBodySnippetQueryGate, body_snippet_elapsed_ms);
  const bool title_only_within_gate =
      kernel::benchmarks::report_gate(kernel::benchmarks::kTitleOnlyQueryGate, title_only_search_elapsed_ms);
  const bool shallow_page_within_gate =
      kernel::benchmarks::report_gate(kernel::benchmarks::kShallowPageQueryGate, shallow_page_elapsed_ms);
  const bool deep_offset_within_gate =
      kernel::benchmarks::report_gate(kernel::benchmarks::kDeepOffsetQueryGate, deep_offset_elapsed_ms);
  const bool filtered_note_within_gate =
      kernel::benchmarks::report_gate(kernel::benchmarks::kFilteredNoteQueryGate, filtered_note_elapsed_ms);
  const bool attachment_path_within_gate =
      kernel::benchmarks::report_gate(kernel::benchmarks::kAttachmentPathQueryGate, attachment_path_elapsed_ms);
  const bool attachment_catalog_within_gate =
      kernel::benchmarks::report_gate(kernel::benchmarks::kAttachmentCatalogQueryGate, attachment_catalog_elapsed_ms);
  const bool attachment_lookup_within_gate =
      kernel::benchmarks::report_gate(kernel::benchmarks::kAttachmentLookupQueryGate, attachment_lookup_elapsed_ms);
  const bool note_attachment_refs_within_gate = kernel::benchmarks::report_gate(
      kernel::benchmarks::kNoteAttachmentRefsQueryGate,
      note_attachment_refs_elapsed_ms);
  const bool attachment_referrers_within_gate = kernel::benchmarks::report_gate(
      kernel::benchmarks::kAttachmentReferrersQueryGate,
      attachment_referrers_elapsed_ms);
  const bool pdf_source_refs_within_gate = kernel::benchmarks::report_gate(
      kernel::benchmarks::kPdfSourceRefsQueryGate,
      pdf_source_refs_elapsed_ms);
  const bool pdf_referrers_within_gate = kernel::benchmarks::report_gate(
      kernel::benchmarks::kPdfReferrersQueryGate,
      pdf_referrers_elapsed_ms);
  const bool all_kind_within_gate =
      kernel::benchmarks::report_gate(kernel::benchmarks::kAllKindQueryGate, all_kind_elapsed_ms);
  const bool ranked_title_within_gate =
      kernel::benchmarks::report_gate(kernel::benchmarks::kRankedTitleQueryGate, ranked_title_elapsed_ms);
  const bool ranked_tag_boost_within_gate =
      kernel::benchmarks::report_gate(kernel::benchmarks::kRankedTagBoostQueryGate, ranked_tag_boost_elapsed_ms);
  const bool ranked_all_kind_within_gate =
      kernel::benchmarks::report_gate(kernel::benchmarks::kRankedAllKindQueryGate, ranked_all_kind_elapsed_ms);
  const bool backlink_within_gate =
      kernel::benchmarks::report_gate(kernel::benchmarks::kBacklinkQueryGate, backlink_elapsed_ms);

  std::cout << " query_benchmark note_count=" << note_count
            << " filter_note_count=" << filter_note_count
            << " iterations=" << iterations
            << " gate_passed=" << (tag_within_gate && title_within_gate && body_within_gate &&
                                        body_snippet_within_gate && title_only_within_gate &&
                                        shallow_page_within_gate && deep_offset_within_gate &&
                                        filtered_note_within_gate && attachment_path_within_gate &&
                                        attachment_catalog_within_gate &&
                                        attachment_lookup_within_gate &&
                                        note_attachment_refs_within_gate &&
                                        attachment_referrers_within_gate &&
                                        pdf_source_refs_within_gate &&
                                        pdf_referrers_within_gate &&
                                        all_kind_within_gate &&
                                        ranked_title_within_gate &&
                                        ranked_tag_boost_within_gate &&
                                        ranked_all_kind_within_gate &&
                                        backlink_within_gate
                                        ? "true"
                                        : "false")
            << "\n";

  kernel_close(handle);
  std::filesystem::remove_all(vault);
  return tag_within_gate && title_within_gate && body_within_gate &&
                 body_snippet_within_gate && title_only_within_gate &&
                 shallow_page_within_gate && deep_offset_within_gate &&
                 filtered_note_within_gate && attachment_path_within_gate &&
                 attachment_catalog_within_gate &&
                 attachment_lookup_within_gate &&
                 note_attachment_refs_within_gate &&
                 attachment_referrers_within_gate &&
                 pdf_source_refs_within_gate &&
                 pdf_referrers_within_gate &&
                 all_kind_within_gate &&
                 ranked_title_within_gate &&
                 ranked_tag_boost_within_gate &&
                 ranked_all_kind_within_gate &&
                 backlink_within_gate
             ? 0
             : 1;
}
