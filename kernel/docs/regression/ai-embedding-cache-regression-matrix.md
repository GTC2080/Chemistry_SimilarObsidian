<!-- Reason: This matrix pins the storage and ranking coverage for the kernel-owned AI embedding cache. -->

# AI Embedding Cache Regression Matrix

The repository must retain regression coverage for:

- kernel schema version includes `ai_embedding_cache`
- `ai_embedding_cache` has `note_rel_path` as the primary key
- `ai_embedding_cache` stores note title, absolute path, created timestamp,
  updated timestamp, and optional vector BLOB
- `idx_ai_embedding_cache_has_embedding` exists for vector-present queries
- metadata upsert inserts new note cache rows
- metadata upsert resets an existing vector to prevent stale semantic hits
- timestamp query returns records sorted by relative path
- full refresh job preparation filters ignored roots in kernel
- full refresh job preparation returns only missing/stale/indexable Markdown note
  content when not forced
- forced refresh job preparation returns indexable Markdown note content even
  when cache metadata is fresh
- refresh job preparation upserts metadata for returned jobs before returning to
  the host
- changed refresh job preparation normalizes/deduplicates changed paths,
  refreshes the kernel note catalog for externally changed Markdown files, and
  returns jobs only for those changed note paths
- vector update stores f32 values through the kernel-owned BLOB codec
- top-k query ranks notes by cosine similarity
- top-k query preserves note relative path and title in `kernel_search_results`
- top-k query honors the excluded active-note relative path
- clear removes vector payloads while preserving metadata rows
- delete removes metadata and vector data together
- delete reports no-op when the relative path is not present
- invalid handles, null output pointers, empty relative paths, empty vectors,
  null vectors, and zero top-k limits are rejected
- Tauri `index_vault_content`, `index_changed_entries`, and
  `rebuild_vector_index` obtain embedding refresh jobs through the sealed kernel
  bridge; Rust only performs external embedding requests and writes vectors back
- Tauri `semantic_search`, `get_related_notes`, `get_related_notes_raw`, and
  `ask_vault` query semantic top-k notes through the sealed kernel bridge
- Tauri Rust does not keep `db/embeddings.rs`, `ai/vector_cache.rs`, or
  `ai/similarity.rs`
- Tauri Rust does not keep `cmd_vault.rs` loops for note timestamp comparison,
  note content reads, content indexability checks, or embedding metadata upserts
