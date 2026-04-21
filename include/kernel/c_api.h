/* Reason: This file exposes the frozen minimal C ABI entry points for the Phase 1 kernel skeleton. */

#pragma once

#include "kernel/types.h"

#ifdef __cplusplus
extern "C" {
#endif

kernel_status kernel_open_vault(const char* vault_path, kernel_handle** out_handle);
kernel_status kernel_close(kernel_handle* handle);
kernel_status kernel_get_state(kernel_handle* handle, kernel_state_snapshot* out_state);
kernel_status kernel_get_rebuild_status(
    kernel_handle* handle,
    kernel_rebuild_status_snapshot* out_status);
kernel_status kernel_read_note(
    kernel_handle* handle,
    const char* rel_path,
    kernel_owned_buffer* out_buffer,
    kernel_note_metadata* out_metadata);
kernel_status kernel_write_note(
    kernel_handle* handle,
    const char* rel_path,
    const char* utf8_text,
    size_t text_size,
    const char* expected_revision,
    kernel_note_metadata* out_metadata,
    kernel_write_disposition* out_disposition);
kernel_status kernel_search_notes(
    kernel_handle* handle,
    const char* query,
    kernel_search_results* out_results);
kernel_status kernel_search_notes_limited(
    kernel_handle* handle,
    const char* query,
    size_t limit,
    kernel_search_results* out_results);
kernel_status kernel_query_search(
    kernel_handle* handle,
    const kernel_search_query* request,
    kernel_search_page* out_page);
kernel_status kernel_query_tag_notes(
    kernel_handle* handle,
    const char* tag,
    size_t limit,
    kernel_search_results* out_results);
kernel_status kernel_query_backlinks(
    kernel_handle* handle,
    const char* rel_path,
    size_t limit,
    kernel_search_results* out_results);
kernel_status kernel_query_attachments(
    kernel_handle* handle,
    size_t limit,
    kernel_attachment_list* out_attachments);
kernel_status kernel_get_attachment(
    kernel_handle* handle,
    const char* attachment_rel_path,
    kernel_attachment_record* out_attachment);
kernel_status kernel_query_note_attachment_refs(
    kernel_handle* handle,
    const char* note_rel_path,
    size_t limit,
    kernel_attachment_list* out_attachments);
kernel_status kernel_query_attachment_referrers(
    kernel_handle* handle,
    const char* attachment_rel_path,
    size_t limit,
    kernel_attachment_referrers* out_referrers);
kernel_status kernel_list_note_attachments(
    kernel_handle* handle,
    const char* note_rel_path,
    kernel_attachment_refs* out_refs);
kernel_status kernel_get_attachment_metadata(
    kernel_handle* handle,
    const char* attachment_rel_path,
    kernel_attachment_metadata* out_metadata);
kernel_status kernel_start_rebuild_index(kernel_handle* handle);
kernel_status kernel_wait_for_rebuild(kernel_handle* handle, uint32_t timeout_ms);
kernel_status kernel_join_rebuild_index(kernel_handle* handle);
kernel_status kernel_rebuild_index(kernel_handle* handle);
kernel_status kernel_export_diagnostics(kernel_handle* handle, const char* output_path);
void kernel_free_buffer(kernel_owned_buffer* buffer);
void kernel_free_search_results(kernel_search_results* results);
void kernel_free_search_page(kernel_search_page* page);
void kernel_free_attachment_record(kernel_attachment_record* attachment);
void kernel_free_attachment_list(kernel_attachment_list* attachments);
void kernel_free_attachment_referrers(kernel_attachment_referrers* referrers);
void kernel_free_attachment_refs(kernel_attachment_refs* refs);

#ifdef __cplusplus
}
#endif
