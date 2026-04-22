// Reason: This file keeps Track 4 domain-query benchmark loops separate so the
// main query benchmark stays readable as more formal public surfaces arrive.

#include "benchmarks/query/query_benchmark_domain.h"

#include "benchmarks/benchmark_thresholds.h"

#include <chrono>
#include <iostream>
#include <string>

namespace {

bool expect_ok(const kernel_status status, const char* operation) {
  if (status.code == KERNEL_OK) {
    return true;
  }
  std::cerr << operation << " failed with code " << status.code << "\n";
  return false;
}

}  // namespace

namespace kernel::benchmarks::query {

bool run_domain_query_benchmarks(
    kernel_handle* handle,
    const DomainBenchmarkConfig& config,
    const int iterations) {
  const auto attachment_metadata_start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i) {
    kernel_domain_metadata_list entries{};
    if (!expect_ok(
            kernel_query_attachment_domain_metadata(
                handle,
                config.attachment_rel_path,
                8,
                &entries),
            "attachment domain metadata query")) {
      return false;
    }
    if (entries.count != 3 ||
        std::string(entries.entries[0].carrier_key) != config.attachment_rel_path ||
        std::string(entries.entries[0].key_name) != "carrier_surface") {
      std::cerr << "attachment domain metadata query returned unexpected state\n";
      return false;
    }
    kernel_free_domain_metadata_list(&entries);
  }
  const auto attachment_metadata_end = std::chrono::steady_clock::now();

  const auto pdf_metadata_start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i) {
    kernel_domain_metadata_list entries{};
    if (!expect_ok(
            kernel_query_pdf_domain_metadata(
                handle,
                config.pdf_rel_path,
                8,
                &entries),
            "pdf domain metadata query")) {
      return false;
    }
    if (entries.count != 7 ||
        std::string(entries.entries[0].carrier_key) != config.pdf_rel_path ||
        std::string(entries.entries[0].key_name) != "carrier_surface") {
      std::cerr << "pdf domain metadata query returned unexpected state\n";
      return false;
    }
    kernel_free_domain_metadata_list(&entries);
  }
  const auto pdf_metadata_end = std::chrono::steady_clock::now();

  const auto attachment_objects_start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i) {
    kernel_domain_object_list objects{};
    if (!expect_ok(
            kernel_query_attachment_domain_objects(
                handle,
                config.attachment_rel_path,
                4,
                &objects),
            "attachment domain objects query")) {
      return false;
    }
    if (objects.count != 1 ||
        std::string(objects.objects[0].domain_object_key) != config.attachment_domain_object_key ||
        objects.objects[0].state != KERNEL_DOMAIN_OBJECT_PRESENT) {
      std::cerr << "attachment domain objects query returned unexpected state\n";
      return false;
    }
    kernel_free_domain_object_list(&objects);
  }
  const auto attachment_objects_end = std::chrono::steady_clock::now();

  const auto pdf_objects_start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i) {
    kernel_domain_object_list objects{};
    if (!expect_ok(
            kernel_query_pdf_domain_objects(
                handle,
                config.pdf_rel_path,
                4,
                &objects),
            "pdf domain objects query")) {
      return false;
    }
    if (objects.count != 1 ||
        std::string(objects.objects[0].domain_object_key) != config.pdf_domain_object_key ||
        objects.objects[0].state != KERNEL_DOMAIN_OBJECT_PRESENT) {
      std::cerr << "pdf domain objects query returned unexpected state\n";
      return false;
    }
    kernel_free_domain_object_list(&objects);
  }
  const auto pdf_objects_end = std::chrono::steady_clock::now();

  const auto object_lookup_start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i) {
    kernel_domain_object_descriptor object{};
    if (!expect_ok(
            kernel_get_domain_object(
                handle,
                config.pdf_domain_object_key,
                &object),
            "domain object lookup query")) {
      return false;
    }
    if (std::string(object.domain_object_key) != config.pdf_domain_object_key ||
        object.state != KERNEL_DOMAIN_OBJECT_PRESENT) {
      std::cerr << "domain object lookup query returned unexpected state\n";
      return false;
    }
    kernel_free_domain_object_descriptor(&object);
  }
  const auto object_lookup_end = std::chrono::steady_clock::now();

  const auto note_source_refs_start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i) {
    kernel_domain_source_refs refs{};
    if (!expect_ok(
            kernel_query_note_domain_source_refs(
                handle,
                config.note_source_rel_path,
                4,
                &refs),
            "domain note source refs query")) {
      return false;
    }
    if (refs.count != 2 ||
        std::string(refs.refs[0].target_object_key) != config.pdf_domain_object_key ||
        refs.refs[0].state != KERNEL_DOMAIN_REF_RESOLVED) {
      std::cerr << "domain note source refs query returned unexpected state\n";
      return false;
    }
    kernel_free_domain_source_refs(&refs);
  }
  const auto note_source_refs_end = std::chrono::steady_clock::now();

  const auto object_referrers_start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i) {
    kernel_domain_referrers referrers{};
    if (!expect_ok(
            kernel_query_domain_object_referrers(
                handle,
                config.pdf_domain_object_key,
                4,
                &referrers),
            "domain object referrers query")) {
      return false;
    }
    if (referrers.count != 3 ||
        std::string(referrers.referrers[0].note_rel_path) != "pdf/bench-referrer.md" ||
        referrers.referrers[0].state != KERNEL_DOMAIN_REF_RESOLVED) {
      std::cerr << "domain object referrers query returned unexpected state\n";
      return false;
    }
    kernel_free_domain_referrers(&referrers);
  }
  const auto object_referrers_end = std::chrono::steady_clock::now();

  const auto attachment_metadata_elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          attachment_metadata_end - attachment_metadata_start)
          .count();
  const auto pdf_metadata_elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          pdf_metadata_end - pdf_metadata_start)
          .count();
  const auto attachment_objects_elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          attachment_objects_end - attachment_objects_start)
          .count();
  const auto pdf_objects_elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          pdf_objects_end - pdf_objects_start)
          .count();
  const auto object_lookup_elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          object_lookup_end - object_lookup_start)
          .count();
  const auto note_source_refs_elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          note_source_refs_end - note_source_refs_start)
          .count();
  const auto object_referrers_elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          object_referrers_end - object_referrers_start)
          .count();

  const bool attachment_metadata_within_gate = report_gate(
      kDomainAttachmentMetadataQueryGate,
      attachment_metadata_elapsed_ms);
  const bool pdf_metadata_within_gate =
      report_gate(kDomainPdfMetadataQueryGate, pdf_metadata_elapsed_ms);
  const bool attachment_objects_within_gate = report_gate(
      kDomainAttachmentObjectsQueryGate,
      attachment_objects_elapsed_ms);
  const bool pdf_objects_within_gate =
      report_gate(kDomainPdfObjectsQueryGate, pdf_objects_elapsed_ms);
  const bool object_lookup_within_gate =
      report_gate(kDomainObjectLookupQueryGate, object_lookup_elapsed_ms);
  const bool note_source_refs_within_gate = report_gate(
      kDomainNoteSourceRefsQueryGate,
      note_source_refs_elapsed_ms);
  const bool object_referrers_within_gate = report_gate(
      kDomainObjectReferrersQueryGate,
      object_referrers_elapsed_ms);

  return attachment_metadata_within_gate &&
         pdf_metadata_within_gate &&
         attachment_objects_within_gate &&
         pdf_objects_within_gate &&
         object_lookup_within_gate &&
         note_source_refs_within_gate &&
         object_referrers_within_gate;
}

}  // namespace kernel::benchmarks::query
