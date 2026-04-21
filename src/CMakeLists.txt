# Reason: This file defines the minimal kernel library target and the focused implementation units it owns.

add_library(
  chem_kernel
  STATIC
  ../third_party/sqlite/sqlite3.c
  impl/core/kernel_shared.cpp
  impl/core/kernel_runtime.cpp
  impl/core/kernel_watcher_runtime.cpp
  impl/core/kernel_rebuild.cpp
  impl/core/kernel_note_io.cpp
  impl/core/kernel_query_api.cpp
  impl/core/kernel_attachment_api_shared.cpp
  impl/core/kernel_attachment_public_api.cpp
  impl/core/kernel_attachment_legacy_api.cpp
  impl/core/kernel_diagnostics.cpp
  impl/diagnostics/logger.cpp
  impl/index/refresh_paths.cpp
  impl/index/refresh_rescan.cpp
  impl/parser/parser.cpp
  impl/recovery/journal_format.cpp
  impl/recovery/journal_scan.cpp
  impl/recovery/journal_startup_recovery.cpp
  impl/search/search.cpp
  impl/search/search_detail.cpp
  impl/search/search_note_queries.cpp
  impl/search/search_attachment_queries.cpp
  impl/storage/storage_sqlite_helpers.cpp
  impl/storage/storage_connection.cpp
  impl/storage/storage_schema.cpp
  impl/storage/storage_note_upsert.cpp
  impl/storage/storage_note_delete_rename.cpp
  impl/storage/storage_attachment_state.cpp
  impl/storage/storage_attachment_queries.cpp
  impl/storage/storage_attachment_diagnostics_queries.cpp
  impl/storage/storage_note_queries.cpp
  impl/watcher/integration.cpp
  impl/watcher/session.cpp
  impl/watcher/watcher.cpp
  impl/watcher/windows_decode.cpp
  impl/platform/windows/platform_windows.cpp
  impl/vault/revision.cpp
  impl/vault/state_paths.cpp
)

target_include_directories(
  chem_kernel
  PUBLIC
    ${PROJECT_SOURCE_DIR}/include
  PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/src/internal
    ${PROJECT_SOURCE_DIR}/third_party/sqlite
)

target_compile_features(chem_kernel PUBLIC cxx_std_20)

if(WIN32)
  target_compile_definitions(chem_kernel PRIVATE KERNEL_PLATFORM_WINDOWS=1)
endif()

target_compile_definitions(chem_kernel PRIVATE SQLITE_ENABLE_FTS5=1)
