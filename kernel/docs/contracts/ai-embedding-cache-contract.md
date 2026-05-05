<!-- Reason: This contract freezes the kernel-owned replacement for the former Tauri Rust embedding DB and in-memory vector cache. -->

# AI Embedding Cache Contract

## Scope

The kernel owns the persistent AI embedding cache and semantic top-k note
retrieval surface.

This surface replaces the former Tauri Rust `db/embeddings.rs`,
`ai/vector_cache.rs`, and local cosine-similarity path. Hosts may still perform
network embedding requests, but they must store note metadata, vector payloads,
timestamp refresh data, deletes, clears, and top-k retrieval through the kernel.

## ABI

Frozen entrypoints:

- `kernel_upsert_ai_embedding_note_metadata(handle, metadata)`
- `kernel_query_ai_embedding_note_timestamps(handle, out_timestamps)`
- `kernel_update_ai_embedding(handle, note_rel_path, values, value_count)`
- `kernel_clear_ai_embeddings(handle)`
- `kernel_delete_ai_embedding_note(handle, note_rel_path, out_deleted)`
- `kernel_query_ai_embedding_top_notes(handle, query_values, query_value_count, exclude_rel_path, limit, out_results)`
- `kernel_free_ai_embedding_timestamp_list(out_timestamps)`
- `kernel_free_search_results(out_results)`

The storage table is kernel schema-owned:

- table: `ai_embedding_cache`
- primary key: `note_rel_path`
- metadata columns: `note_title`, `absolute_path`, `created_at`, `updated_at`
- vector column: `embedding`
- partial vector index: `idx_ai_embedding_cache_has_embedding`

## Rules

Frozen rules:

- metadata upsert requires a non-empty `rel_path`, `title`, and `absolute_path`
- metadata upsert writes `created_at` and `updated_at` exactly as supplied by the
  host note catalog workflow
- metadata upsert clears the existing vector for that note, so a refreshed note
  cannot keep a stale embedding
- timestamp queries return `(rel_path, updated_at)` records sorted by `rel_path`
- vector updates require a non-empty `rel_path`, non-null values, and non-zero
  value count
- clear removes vector payloads but keeps note metadata rows
- delete removes the metadata row and vector together and reports whether a row
  was removed
- top-k queries consider only rows with a vector payload
- top-k queries use cosine similarity and may exclude the currently open note by
  relative path
- top-k result shape reuses `kernel_search_results` so hosts can keep existing
  semantic-search DTO mapping

## Host Boundary

Tauri Rust may:

- obtain note metadata and content from the kernel note catalog/read surfaces
- call the external embedding provider
- schedule background embedding tasks
- map kernel search hits into existing Tauri command DTOs

Tauri Rust must not:

- create, query, or mutate a local embedding SQL table
- keep `VectorCacheState` or another in-memory semantic top-k cache
- reimplement cosine-similarity ranking for persisted note embeddings
- read embedding timestamps from Rust-owned DB rows
- delete embedding rows through Rust-owned SQL lifecycle code

