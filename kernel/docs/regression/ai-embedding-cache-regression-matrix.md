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
- vector update stores f32 values through the kernel-owned BLOB codec
- top-k query ranks notes by cosine similarity
- top-k query preserves note relative path and title in `kernel_search_results`
- top-k query honors the excluded active-note relative path
- clear removes vector payloads while preserving metadata rows
- delete removes metadata and vector data together
- delete reports no-op when the relative path is not present
- invalid handles, null output pointers, empty relative paths, empty vectors,
  null vectors, and zero top-k limits are rejected
- Tauri `index_vault_content`, `index_changed_entries`, `write_note`, and
  `rebuild_vector_index` write embedding metadata/vectors through the sealed
  kernel bridge
- Tauri `semantic_search`, `get_related_notes`, `get_related_notes_raw`, and
  `ask_vault` query semantic top-k notes through the sealed kernel bridge
- Tauri Rust does not keep `db/embeddings.rs`, `ai/vector_cache.rs`, or
  `ai/similarity.rs`

